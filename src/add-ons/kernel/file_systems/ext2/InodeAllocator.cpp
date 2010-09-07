/*
 * Copyright 2001-2010, Haiku Inc. All rights reserved.
 * This file may be used under the terms of the MIT License.
 *
 * Authors:
 *		Janito V. Ferreira Filho
 */


#include "InodeAllocator.h"

#include <util/AutoLock.h>

#include "BitmapBlock.h"
#include "Inode.h"
#include "Volume.h"


//#define TRACE_EXT2
#ifdef TRACE_EXT2
#	define TRACE(x...) dprintf("\33[34mext2:\33[0m " x)
#else
#	define TRACE(x...) ;
#endif


InodeAllocator::InodeAllocator(Volume* volume)
	:
	fVolume(volume)
{
	mutex_init(&fLock, "ext2 inode allocator");
}


InodeAllocator::~InodeAllocator()
{
	mutex_destroy(&fLock);
}


/*virtual*/ status_t
InodeAllocator::New(Transaction& transaction, Inode* parent, int32 mode,
	ino_t& id)
{
	// Apply allocation policy
	uint32 preferredBlockGroup = parent == NULL ? parent->ID()
		/ parent->GetVolume()->InodesPerGroup() : 0;
	
	return _Allocate(transaction, preferredBlockGroup, S_ISDIR(mode), id);
}


/*virtual*/ status_t
InodeAllocator::Free(Transaction& transaction, ino_t id, bool isDirectory)
{
	TRACE("InodeAllocator::Free(%d, %c)\n", (int)id, isDirectory ? 't' : 'f');
	MutexLocker lock(fLock);

	uint32 numInodes = fVolume->InodesPerGroup();
	uint32 blockGroup = (id - 1) / numInodes;
	ext2_block_group* group;

	status_t status = fVolume->GetBlockGroup(blockGroup, &group);
	if (status != B_OK)
		return status;

	if (blockGroup == fVolume->NumGroups() - 1)
		numInodes = fVolume->NumInodes() - blockGroup * numInodes;

	TRACE("InodeAllocator::Free(): Updating block group data\n");
	group->SetFreeInodes(group->FreeInodes() + 1);
	if (isDirectory)
		group->SetUsedDirectories(group->UsedDirectories() - 1);

	status = fVolume->WriteBlockGroup(transaction, blockGroup);
	if (status != B_OK)
		return status;

	return _UnmarkInBitmap(transaction, group->InodeBitmap(), numInodes, id);
}


status_t
InodeAllocator::_Allocate(Transaction& transaction, uint32 preferredBlockGroup,
	bool isDirectory, ino_t& id)
{
	MutexLocker lock(fLock);

	uint32 blockGroup = preferredBlockGroup;
	uint32 lastBlockGroup = fVolume->NumGroups() - 1;

	for (int i = 0; i < 2; ++i) {
		for (; blockGroup < lastBlockGroup; ++blockGroup) {
			ext2_block_group* group;

			status_t status = fVolume->GetBlockGroup(blockGroup, &group);
			if (status != B_OK)
				return status;

			uint32 freeInodes = group->FreeInodes();
			if (freeInodes != 0) {
				group->SetFreeInodes(freeInodes - 1);
				if (isDirectory)
					group->SetUsedDirectories(group->UsedDirectories() + 1);

				status = fVolume->WriteBlockGroup(transaction, blockGroup);
				if (status != B_OK)
					return status;

				return _MarkInBitmap(transaction, group->InodeBitmap(),
					blockGroup, fVolume->InodesPerGroup(), id);
			}
		}

		if (i == 0) {
			ext2_block_group* group;

			status_t status = fVolume->GetBlockGroup(blockGroup, &group);
			if (status != B_OK)
				return status;

			uint32 freeInodes = group->FreeInodes();
			if (group->FreeInodes() != 0) {
				group->SetFreeInodes(freeInodes - 1);

				return _MarkInBitmap(transaction, group->InodeBitmap(),
					blockGroup, fVolume->NumInodes()
						- blockGroup * fVolume->InodesPerGroup(), id);
			}
		}

		blockGroup = 0;
		lastBlockGroup = preferredBlockGroup;
	}

	return B_DEVICE_FULL;
}


status_t
InodeAllocator::_MarkInBitmap(Transaction& transaction, uint32 bitmapBlock,
	uint32 blockGroup, uint32 numInodes, ino_t& id)
{
	BitmapBlock inodeBitmap(fVolume, numInodes);

	if (!inodeBitmap.SetToWritable(transaction, bitmapBlock)) {
		TRACE("Unable to open inode bitmap (block number: %lu) for block group "
			"%lu\n", bitmapBlock, blockGroup);
		return B_IO_ERROR;
	}

	uint32 pos = 0;
	inodeBitmap.FindNextUnmarked(pos);

	if (pos == inodeBitmap.NumBits()) {
		TRACE("Even though the block group %lu indicates there are free "
			"inodes, no unmarked bit was found in the inode bitmap at block "
			"%lu.", blockGroup, bitmapBlock);
		return B_ERROR;
	}

	if (!inodeBitmap.Mark(pos, 1)) {
		TRACE("Failed to mark bit %lu at bitmap block %lu\n", pos,
			bitmapBlock);
		return B_BAD_DATA;
	}

	id = pos + blockGroup * fVolume->InodesPerGroup() + 1;

	return B_OK;
}


status_t
InodeAllocator::_UnmarkInBitmap(Transaction& transaction, uint32 bitmapBlock,
	uint32 numInodes, ino_t id)
{
	BitmapBlock inodeBitmap(fVolume, numInodes);

	if (!inodeBitmap.SetToWritable(transaction, bitmapBlock)) {
		TRACE("Unable to open inode bitmap at block %lu\n", bitmapBlock);
		return B_IO_ERROR;
	}

	uint32 pos = (id - 1) % fVolume->InodesPerGroup();
	if (!inodeBitmap.Unmark(pos, 1)) {
		TRACE("Unable to unmark bit %lu in inode bitmap block %lu\n", pos,
			bitmapBlock);
		return B_BAD_DATA;
	}

	return B_OK;
}
