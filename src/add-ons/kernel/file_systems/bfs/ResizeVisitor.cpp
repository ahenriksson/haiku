#include "ResizeVisitor.h"

#include "BPlusTree.h"
#include "Index.h"
#include "Inode.h"
#include "Journal.h"


status_t
ResizeVisitor::StartResize(off_t newSize)
{
	uint32 blockSize = GetVolume()->BlockSize();
	uint32 blockShift = GetVolume()->BlockShift();

	// round up to a whole block
	off_t newNumBlocks = (newSize + blockSize - 1) >> blockShift;

	off_t diskSize;
	status_t status = _TemporaryGetDiskSize(diskSize);
	if (status != B_OK)
		return status;

	if (diskSize < (newNumBlocks << blockShift))
		return B_BAD_VALUE;

	off_t newLogSize = 2048;
	if (newNumBlocks <= 20480)
		newLogSize = 512;
	if (newSize > 1LL * 1024 * 1024 * 1024)
		newLogSize = 4096;

	fBitmapBlocks = (newNumBlocks + blockSize * 8 - 1) / (blockSize * 8);
	
	// the new size is limited by what we can fit into the first allocation
	// group
	off_t newReservedLength = 1 + fBitmapBlocks + newLogSize;
	if (newReservedLength > (1UL << GetVolume()->AllocationGroupShift()))
		return B_BAD_VALUE;

	fNewLog.SetTo(0, 1 + fBitmapBlocks, newLogSize);

	fBeginBlock = newReservedLength;
	fEndBlock = newNumBlocks;

	if (newNumBlocks < GetVolume()->NumBlocks()) {
		fShrinking = true;
	} else {
		fShrinking = false;
	} // aTODO handle no resize needed / no data moving needed
		//if (newReservedLength
			//> GetVolume()->Log().Start() + GetVolume()->Log().Length()) {

	Start(VISIT_REGULAR | VISIT_INDICES | VISIT_REMOVED
		/*| VISIT_ATTRIBUTE_DIRECTORIES can't move these yet*/);

	/*
	if (newNumBlocks < GetVolume()->NumBlocks()) {
		// shrinking
		//move data to allowed
		//do resize
		//move journal(?)
	} else if (newNumBlocks > GetVolume()->NumBlocks()) {
		// growing
		if (newReservedLength
			> GetVolume()->Log().Start() + GetVolume()->Log().Length()) {
			//move data out of the area we need as reserved
		}
		//move journal
		//do resize
	} else {
		// perhaps we should inform the user that no resize happened?
	}
	*/
	return B_OK;
}


status_t
ResizeVisitor::FinishResize()
{
	status_t status;

	if (fShrinking) {
		status = _ResizeVolume();
		if (status != B_OK)
			return status;

		status = GetVolume()->GetJournal(0)->MoveLog(fNewLog);
		if (status != B_OK)
			return status;
	} else {
		status = GetVolume()->GetJournal(0)->MoveLog(fNewLog);
		if (status != B_OK)
			return status;

		status = _ResizeVolume();
		if (status != B_OK)
			return status;
	}
	return B_OK;
}


status_t
ResizeVisitor::VisitInode(Inode* inode, const char* treeName)
{
	status_t status;
	off_t inodeBlock = inode->BlockNumber();

	// start by moving the inode so we can place the stream close to it
	// if possible
	if (inodeBlock < fBeginBlock || inodeBlock >= fEndBlock) {
		status = _MoveInode(inode);
		if (status != B_OK)
			return status;
	}

	// move the stream if necessary
	bool inRange;
	status = inode->StreamInRange(fBeginBlock, fEndBlock, inRange);
	if (status != B_OK)
		return status;

	if (!inRange) {
		status = inode->MoveStream(fBeginBlock, fEndBlock);
		if (status != B_OK)
			return status;
	}

	return B_OK;
}


status_t
ResizeVisitor::OpenInodeFailed(status_t reason, ino_t id, Inode* parent,
	char* treeName, TreeIterator* iterator)
{
	if (id < fBeginBlock || id >= fEndBlock) {
		FATAL(("Could not open inode that needed to be moved at %" B_PRIdOFF
			"\n", id));
		return reason;
	}

	// if the inode is not outside the allowed area, we might be able to
	// resize anyways
	FATAL(("Could not open inode at %" B_PRIdOFF "\n", id));

	if (fForce)
		return B_OK;

	return reason;
}


status_t
ResizeVisitor::OpenBPlusTreeFailed(Inode* inode)
{
	FATAL(("Could not open b+tree from inode at %" B_PRIdOFF "\n",
		inode->ID()));

	if (fForce)
		return B_OK;

	return B_ERROR;
}


status_t
ResizeVisitor::TreeIterationFailed(status_t reason, Inode* parent)
{
	FATAL(("Tree iteration failed in parent at %" B_PRIdOFF "\n",
		parent->ID()));

	if (fForce)
		return B_OK;

	return reason;
}


status_t
ResizeVisitor::_ResizeVolume()
{
	status_t status;
	BlockAllocator& allocator = GetVolume()->Allocator();

	// check that the end blocks are free
	if (fEndBlock < GetVolume()->NumBlocks()) {
		status = allocator.CheckBlocks(fEndBlock,
			GetVolume()->NumBlocks() - fEndBlock, false);
		if (status != B_OK)
			return status;
	}
	
	// make sure we have space to resize the bitmap
	if (1 + fBitmapBlocks > GetVolume()->Log().Start()) {
		return B_ERROR;
	}
	
	// clear bitmap blocks - aTODO maybe not use a transaction?
	Transaction transaction(GetVolume(), 0);

	CachedBlock cached(GetVolume());
	for (off_t block = 1 + GetVolume()->NumBitmapBlocks();
		block < 1 + fBitmapBlocks; block++) {
		uint8* buffer = cached.SetToWritable(transaction, block);
		if (buffer == NULL)
			return B_IO_ERROR;

		memset(buffer, 0, GetVolume()->BlockSize());
	}
	cached.Unset();

	status = transaction.Done();
	if (status != B_OK)
		return status;
	
	// update superblock and volume information
	disk_super_block& superBlock = GetVolume()->SuperBlock();

	uint32 groupShift = GetVolume()->AllocationGroupShift();
	int32 blocksPerGroup = 1L << groupShift;

	// used to revert the volume super_block changes if we can't write them
	// to disk, so we don't have to convert to and from host endianess
	int64 oldNumBlocks = superBlock.num_blocks;
	int32 oldNumAgs = superBlock.num_ags;

	superBlock.num_blocks = HOST_ENDIAN_TO_BFS_INT64(fEndBlock - fBeginBlock);
	superBlock.num_ags = HOST_ENDIAN_TO_BFS_INT32(
			(fBitmapBlocks + blocksPerGroup - 1) >> groupShift);

	status = GetVolume()->WriteSuperBlock();
	if (status != B_OK) {
		superBlock.num_blocks = oldNumBlocks;
		superBlock.num_ags = oldNumAgs;
		return status;
	}

	// reinitialize block allocator
	status = GetVolume()->Allocator().Reinitialize();
	if (status != B_OK)
		return status;

	return B_OK;
}


status_t
ResizeVisitor::_UpdateIndexReferences(Transaction& transaction, Inode* inode,
	off_t newInodeID)
{
	// update user file attributes
	AttributeIterator iterator(inode);

	char attributeName[B_FILE_NAME_LENGTH];
	size_t nameLength;
	uint32 attributeType;
	ino_t attributeID;

	uint8 key[BPLUSTREE_MAX_KEY_LENGTH];
	size_t keyLength;

	status_t status;
	Index index(GetVolume());

	while (iterator.GetNext(attributeName, &nameLength, &attributeType,
			&attributeID) == B_OK) {
		// ignore attribute if not in index
		if (index.SetTo(attributeName) != B_OK)
			continue;

		status = inode->ReadAttribute(attributeName, attributeType, 0, key,
			&keyLength);
		if (status != B_OK)
			return status;

		status = index.UpdateInode(transaction, key, (uint16)keyLength,
			inode->ID(), newInodeID);
		if (status != B_OK)
			return status;
	}

	// update built-in attributes
	if (inode->InNameIndex()) {
		status = index.SetTo("name");
		if (status != B_OK)
			return status;

		status = inode->GetName((char*)key, BPLUSTREE_MAX_KEY_LENGTH);
		if (status != B_OK)
			return status;

		status = index.UpdateInode(transaction, key, (uint16)strlen((char*)key),
			inode->ID(), newInodeID);
		if (status != B_OK)
			return status;
	}
	if (inode->InSizeIndex()) {
		status = index.SetTo("size");
		if (status != B_OK)
			return status;

		off_t size = inode->Size();
		status = index.UpdateInode(transaction, (uint8*)&size, sizeof(int64),
			inode->ID(), newInodeID);
		if (status != B_OK)
			return status;
	}
	if (inode->InLastModifiedIndex()) {
		status = index.SetTo("last_modified");
		if (status != B_OK)
			return status;

		off_t modified = inode->LastModified();
		status = index.UpdateInode(transaction, (uint8*)&modified,
			sizeof(int64), inode->ID(), newInodeID);
		if (status != B_OK)
			return status;
	}
	return B_OK;

}


status_t
ResizeVisitor::_UpdateParent(Transaction& transaction, Inode* inode,
	off_t newInodeID)
{
	// get name of this inode
	char name[B_FILE_NAME_LENGTH];
	status_t status = inode->GetName(name, B_FILE_NAME_LENGTH);
	if (status != B_OK)
		return status;

	// get Inode of parent
	off_t parentBlock = GetVolume()->ToBlock(inode->Parent());

	Vnode parentVnode(GetVolume(), parentBlock);
	Inode* parent;
	status = parentVnode.Get(&parent);
	if (status != B_OK)
		return status;

	// update inode id in parent
	BPlusTree* tree = parent->Tree();
	return tree->Replace(transaction, (const uint8*)name, (uint16)strlen(name),
		newInodeID);
}


status_t
ResizeVisitor::_MoveInode(Inode* inode)
{
	Transaction transaction(GetVolume(), 0);

	block_run run;
	status_t status = GetVolume()->Allocator().AllocateBlocks(transaction, 0, 0,
		1, 1, run, fBeginBlock, fEndBlock);
		// TODO: use a hint, maybe old position % new volume size?
		//       stuff that originally was in the beginning should probably
		//       stay close to it
	if (status != B_OK)
		return status;

	off_t newInodeID = GetVolume()->ToBlock(run);

	status = inode->Copy(transaction, newInodeID);
	if (status != B_OK)
		return status;

	status = _UpdateParent(transaction, inode, newInodeID);
	if (status != B_OK)
		return status;

	status = _UpdateParent(transaction, inode, newInodeID);
	if (status != B_OK)
		return status;

	status = GetVolume()->Free(transaction, inode->BlockRun());
	if (status != B_OK)
		return status;

	return transaction.Done();
}


status_t
ResizeVisitor::_TemporaryGetDiskSize(off_t& size)
{
	device_geometry geometry;
	if (ioctl(GetVolume()->Device(), B_GET_GEOMETRY, &geometry) < 0) {
		// maybe it's just a file
		struct stat stat;
		if (fstat(GetVolume()->Device(), &stat) < 0)
			return B_ERROR;

		size = stat.st_size;
		return B_OK;
	}

	size = 1LL * geometry.head_count * geometry.cylinder_count
		* geometry.sectors_per_track * geometry.bytes_per_sector;

	return B_OK;
}
