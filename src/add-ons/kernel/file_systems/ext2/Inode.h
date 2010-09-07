/*
 * Copyright 2008, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */
#ifndef INODE_H
#define INODE_H


#include <fs_cache.h>
#include <lock.h>
#include <string.h>

#include "ext2.h"
#include "Volume.h"


class Inode : public TransactionListener {
public:
						Inode(Volume* volume, ino_t id);
						~Inode();

			status_t	InitCheck();

			ino_t		ID() const { return fID; }

			rw_lock*	Lock() { return &fLock; }
			void		WriteLockInTransaction(Transaction& transaction);

			status_t	UpdateNodeFromDisk();
			status_t	WriteBack(Transaction& transaction);

			bool		IsDirectory() const
							{ return S_ISDIR(Mode()); }
			bool		IsFile() const
							{ return S_ISREG(Mode()); }
			bool		IsSymLink() const
							{ return S_ISLNK(Mode()); }
			status_t	CheckPermissions(int accessMode) const;

			bool		IsDeleted() const { return fUnlinked; }

			mode_t		Mode() const { return fNode.Mode(); }
			int32		Flags() const { return fNode.Flags(); }

			off_t		Size() const { return fNode.Size(); }
			time_t		ModificationTime() const
							{ return fNode.ModificationTime(); }
			time_t		CreationTime() const
							{ return fNode.CreationTime(); }
			time_t		AccessTime() const
							{ return fNode.AccessTime(); }

			//::Volume* _Volume() const { return fVolume; }
			Volume*		GetVolume() const { return fVolume; }

			status_t	FindBlock(off_t offset, uint32& block);
			status_t	ReadAt(off_t pos, uint8 *buffer, size_t *length);
			status_t	WriteAt(Transaction& transaction, off_t pos,
							const uint8* buffer, size_t* length);
			status_t	FillGapWithZeros(off_t start, off_t end);

			status_t	Resize(Transaction& transaction, off_t size);

			status_t	AttributeBlockReadAt(off_t pos, uint8 *buffer,
							size_t *length);

			ext2_inode&	Node() { return fNode; }

			status_t	InitDirectory(Transaction& transaction, Inode* parent);

			status_t	Unlink(Transaction& transaction);

	static	status_t	Create(Transaction& transaction, Inode* parent,
							const char* name, int32 mode, int openMode,
							uint8 type, bool* _created = NULL,
							ino_t* _id = NULL, Inode** _inode = NULL,
							fs_vnode_ops* vnodeOps = NULL,
							uint32 publishFlags = 0);

			void*		FileCache() const { return fCache; }
			void*		Map() const { return fMap; }
			status_t	EnableFileCache();
			status_t	DisableFileCache();
			bool		IsFileCacheDisabled() const { return !fCached; }

			status_t	Sync();

protected:
	virtual	void		TransactionDone(bool success);
	virtual	void		RemovedFromTransaction();


private:
						Inode(Volume* volume);
						Inode(const Inode&);
						Inode &operator=(const Inode&);
							// no implementation

			status_t	_EnlargeDataStream(Transaction& transaction,
							off_t size);
			status_t	_ShrinkDataStream(Transaction& transaction, off_t size);


			rw_lock		fLock;
			::Volume*	fVolume;
			ino_t		fID;
			void*		fCache;
			void*		fMap;
			bool		fCached;
			bool		fUnlinked;
			ext2_inode	fNode;
			uint32		fNodeSize;
				// Inodes have a varible size, but the important
				// information is always the same size (except in ext4)
			ext2_xattr_header* fAttributesBlock;
			status_t	fInitStatus;
};


// The Vnode class provides a convenience layer upon get_vnode(), so that
// you don't have to call put_vnode() anymore, which may make code more
// readable in some cases

class Vnode {
public:
	Vnode(Volume* volume, ino_t id)
		:
		fInode(NULL)
	{
		SetTo(volume, id);
	}

	Vnode()
		:
		fStatus(B_NO_INIT),
		fInode(NULL)
	{
	}

	~Vnode()
	{
		Unset();
	}

	status_t InitCheck()
	{
		return fStatus;
	}

	void Unset()
	{
		if (fInode != NULL) {
			put_vnode(fInode->GetVolume()->FSVolume(), fInode->ID());
			fInode = NULL;
			fStatus = B_NO_INIT;
		}
	}

	status_t SetTo(Volume* volume, ino_t id)
	{
		Unset();

		return fStatus = get_vnode(volume->FSVolume(), id, (void**)&fInode);
	}

	status_t Get(Inode** _inode)
	{
		*_inode = fInode;
		return fStatus;
	}

	void Keep()
	{
		dprintf("Vnode::Keep()\n");
		fInode = NULL;
	}

	status_t Publish(Transaction& transaction, Inode* inode,
		fs_vnode_ops* vnodeOps, uint32 publishFlags)
	{
		dprintf("Vnode::Publish()\n");
		Volume* volume = transaction.GetVolume();

		status_t status = B_OK;

		if (!inode->IsSymLink() && volume->ID() >= 0) {
			dprintf("Vnode::Publish(): Publishing vnode: %d, %d, %p, %p, %x, "
				"%x\n", (int)volume->FSVolume(), (int)inode->ID(), inode,
				vnodeOps != NULL ? vnodeOps : &gExt2VnodeOps, (int)inode->Mode(),
				(int)publishFlags);
			status = publish_vnode(volume->FSVolume(), inode->ID(), inode,
				vnodeOps != NULL ? vnodeOps : &gExt2VnodeOps, inode->Mode(),
				publishFlags);
			dprintf("Vnode::Publish(): Result: %s\n", strerror(status));
		}

		if (status == B_OK) {
			dprintf("Vnode::Publish(): Preparing internal data\n");
			fInode = inode;
			fStatus = B_OK;

			cache_add_transaction_listener(volume->BlockCache(),
				transaction.ID(), TRANSACTION_ABORTED, &_TransactionListener,
				inode);
		}

		return status;
	}

private:
	status_t	fStatus;
	Inode*		fInode;

	// TODO: How to apply coding style here?
	static void _TransactionListener(int32 id, int32 event, void* _inode)
	{
		Inode* inode = (Inode*)_inode;

		if (event == TRANSACTION_ABORTED) {
			// TODO: Unpublish?
			panic("Transaction %d aborted, inode %p still exists!\n", (int)id,
				inode);
		}
	}
};

#endif	// INODE_H
