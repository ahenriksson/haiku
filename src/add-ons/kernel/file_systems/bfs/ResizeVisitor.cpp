/*
 * Copyright 2012, Andreas Henriksson, sausageboy@gmail.com
 * This file may be used under the terms of the MIT License.
 */


//! File system resizing


#include "ResizeVisitor.h"

#include "BPlusTree.h"
#include "Index.h"
#include "Inode.h"
#include "Journal.h"


ResizeVisitor::ResizeVisitor(Volume* volume)
	:
	FileSystemVisitor(volume)
{
}


void
ResizeVisitor::StartResize()
{
	_SetError(B_OK);

	uint32 blockSize = GetVolume()->BlockSize();
	uint32 blockShift = GetVolume()->BlockShift();

	// fill in old values in the control
	Control().stats.bitmap_blocks_old = GetVolume()->NumBitmapBlocks();
	Control().stats.log_start_old = GetVolume()->Log().Start();
	Control().stats.log_length_old = GetVolume()->Log().Length();

	Control().stats.block_size = GetVolume()->BlockSize();

	// calculate new values for the log and number of bitmap blocks
	off_t numBlocks = (Control().new_size + blockSize - 1) >> blockShift;
		// round up to a whole block

	uint16 logLength = 2048;
	if (numBlocks <= 20480)
		logLength = 512;
	if (Control().new_size > 1LL * 1024 * 1024 * 1024)
		logLength = 4096;

	fBitmapBlocks = (numBlocks + blockSize * 8 - 1) / (blockSize * 8);
	fNewLog.SetTo(0, 1 + fBitmapBlocks, logLength);

	off_t reservedLength = 1 + fBitmapBlocks + logLength;

	// fill in new values
	Control().stats.bitmap_blocks_new = fBitmapBlocks;
	Control().stats.log_start_new = 1 + fBitmapBlocks;
	Control().stats.log_length_new = logLength;


	// See if the resize is actually possible

	// the new size is limited by what we can fit into the first allocation
	// group
	if (reservedLength > (1UL << GetVolume()->AllocationGroupShift())) {
		_SetError(B_BAD_VALUE, BFS_SIZE_TOO_LARGE);
		return;
	}

	off_t diskSize;
	status_t status = _TemporaryGetDiskSize(diskSize);
	if (status != B_OK) {
		_SetError(status);
		return;
	}

	if (diskSize < (numBlocks << blockShift)) {
		_SetError(B_BAD_VALUE, BFS_DISK_TOO_SMALL);
		return;
	}

	if (GetVolume()->UsedBlocks() > numBlocks) {
		_SetError(B_BAD_VALUE, BFS_NO_SPACE);
		return;
	}

	// stop now if we're only checking out the effects of a resize
	if (Control().flags & BFS_CHECK_RESIZE)
		return;

	// make sure no unsynced transaction causes invalidated blocks to linger
	// in the block cache when we write data with write_pos
	status = GetVolume()->GetJournal(0)->FlushLogAndBlocks();
	if (status != B_OK) {
		_SetError(status);
		return;
	}

	fBeginBlock = reservedLength;

	if (numBlocks < GetVolume()->NumBlocks()) {
		fShrinking = true;
		fEndBlock = numBlocks;
	} else {
		fShrinking = false;
		fEndBlock = GetVolume()->NumBlocks();
	}

	Start(VISIT_REGULAR | VISIT_INDICES | VISIT_REMOVED
		| VISIT_ATTRIBUTE_DIRECTORIES);
}


void
ResizeVisitor::FinishResize()
{
	_SetError(B_OK);

	status_t status;

	if (fShrinking) {
		status = _ResizeVolume();
		if (status != B_OK) {
			_SetError(status, BFS_CHANGE_SIZE_FAILED);
			return;
		}

		status = GetVolume()->GetJournal(0)->MoveLog(fNewLog);
		if (status != B_OK) {
			_SetError(status, BFS_MOVE_LOG_FAILED);
			return;
		}
	} else {
		status = GetVolume()->GetJournal(0)->MoveLog(fNewLog);
		if (status != B_OK) {
			_SetError(status, BFS_MOVE_LOG_FAILED);
			return;
		}

		status = _ResizeVolume();
		if (status != B_OK) {
			_SetError(status, BFS_CHANGE_SIZE_FAILED);
			return;
		}
	}
}


status_t
ResizeVisitor::VisitInode(Inode* inode, const char* treeName)
{
	Control().inode = inode->ID();
	
	// set name
	if (treeName == NULL) {
		if (inode->GetName(Control().name) < B_OK) {
			if (inode->IsContainer())
				strcpy(Control().name, "(dir has no name)");
			else
				strcpy(Control().name, "(node has no name)");
		}
	} else
		strcpy(Control().name, treeName);

	WriteLocker writeLocker(inode->Lock());

	status_t status;
	off_t inodeBlock = inode->BlockNumber();

	// start by moving the inode so we can place the stream close to it
	// if possible
	if (inodeBlock < fBeginBlock || inodeBlock >= fEndBlock) {
		status = mark_vnode_busy(GetVolume()->FSVolume(), inode->ID(), true);

		ino_t oldInodeID = inode->ID();
		off_t newInodeID;

		status = _MoveInode(inode, newInodeID);
		if (status != B_OK) {
			mark_vnode_busy(GetVolume()->FSVolume(), inode->ID(), false);
			_SetError(status, BFS_MOVE_INODE_FAILED);
			return status;
		}

		status = change_vnode_id(GetVolume()->FSVolume(), oldInodeID,
			newInodeID);
		if (status != B_OK) {
			mark_vnode_busy(GetVolume()->FSVolume(), inode->ID(), false);
			_SetError(status, BFS_MOVE_INODE_FAILED);
			return status;
		}

		inode->SetID(newInodeID);

		// accessing the inode with the new ID
		mark_vnode_busy(GetVolume()->FSVolume(), inode->ID(), false);

		Control().stats.inodes_moved++;
	}

	// move the stream if necessary
	bool inRange;
	status = inode->StreamInRange(fBeginBlock, fEndBlock, inRange);
	if (status != B_OK) {
		_SetError(status, BFS_MOVE_STREAM_FAILED);
		return status;
	}

	if (!inRange) {
		status = inode->MoveStream(fBeginBlock, fEndBlock);
		if (status != B_OK) {
			_SetError(status, BFS_MOVE_STREAM_FAILED);
			return status;
		}
		Control().stats.streams_moved++;
	}

	return B_OK;
}


status_t
ResizeVisitor::OpenInodeFailed(status_t reason, ino_t id, Inode* parent,
	char* treeName, TreeIterator* iterator)
{
	FATAL(("Could not open inode at %" B_PRIdOFF "\n", id));

	_SetError(reason);
	return reason;
}


status_t
ResizeVisitor::OpenBPlusTreeFailed(Inode* inode)
{
	FATAL(("Could not open b+tree from inode at %" B_PRIdOFF "\n",
		inode->ID()));

	_SetError(B_ERROR);
	return B_ERROR;
}


status_t
ResizeVisitor::TreeIterationFailed(status_t reason, Inode* parent)
{
	FATAL(("Tree iteration failed in parent at %" B_PRIdOFF "\n",
		parent->ID()));

	_SetError(reason);
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
	if (1 + fBitmapBlocks > GetVolume()->Log().Start())
		return B_ERROR;
	
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

	superBlock.num_blocks = HOST_ENDIAN_TO_BFS_INT64(fEndBlock);
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
ResizeVisitor::_UpdateParent(Transaction& transaction, Inode* inode,
	off_t newInodeID)
{
	// are we the root node or index directory?
	if (inode->BlockRun() == inode->Parent())
		return B_OK;

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
	if (inode->IsAttributeDirectory()) {
		parent->Attributes() = GetVolume()->ToBlockRun(newInodeID);
		return parent->WriteBack(transaction);
	} else {
		BPlusTree* tree = parent->Tree();
		return tree->Replace(transaction, (const uint8*)name,
			(uint16)strlen(name), newInodeID);
	}
}


status_t
ResizeVisitor::_UpdateAttributeDirectory(Transaction& transaction, Inode* inode,
	block_run newInodeRun)
{
	if (inode->Attributes().IsZero())
		return B_OK;

	Vnode vnode(GetVolume(), inode->Attributes());
	Inode* attributeDirectory;

	status_t status = vnode.Get(&attributeDirectory);
	if (status != B_OK)
		return status;

	attributeDirectory->Parent() = newInodeRun;
	return attributeDirectory->WriteBack(transaction);
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
ResizeVisitor::_MoveInode(Inode* inode, off_t& newInodeID)
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

	newInodeID = GetVolume()->ToBlock(run);

	status = inode->Copy(transaction, newInodeID);
	if (status != B_OK)
		return status;

	status = _UpdateParent(transaction, inode, newInodeID);
	if (status != B_OK)
		return status;

	status = _UpdateAttributeDirectory(transaction, inode, run);
	if (status != B_OK)
		return status;

	status = _UpdateIndexReferences(transaction, inode, newInodeID);
	if (status != B_OK)
		return status;

	status = GetVolume()->Free(transaction, inode->BlockRun());
	if (status != B_OK)
		return status;

	return transaction.Done();
}


bool
ResizeVisitor::_ControlValid()
{
	if (Control().magic != BFS_IOCTL_RESIZE_MAGIC) {
		FATAL(("invalid resize_control!\n"));
		return false;
	}

	return true;
}


void
ResizeVisitor::_SetError(status_t status, uint32 failurePoint)
{
	Control().status = status;
	Control().failure_point = failurePoint;
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
