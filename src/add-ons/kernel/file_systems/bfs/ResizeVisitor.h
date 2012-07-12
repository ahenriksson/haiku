#ifndef RESIZE_VISITOR_H
#define RESIZE_VISITOR_H


#include "system_dependencies.h"

#include "FileSystemVisitor.h"


class Inode;
class Transaction;


class ResizeVisitor : public FileSystemVisitor {
public:
								ResizeVisitor(Volume* volume) // aTODO tmp
									:
									FileSystemVisitor(volume),
									fForce(false) {}

	status_t					StartResize(off_t newSize);
	status_t					FinishResize();

	virtual status_t			VisitInode(Inode* inode, const char* treeName);

	virtual status_t			OpenInodeFailed(status_t reason, ino_t id,
									Inode* parent, char* treeName,
									TreeIterator* iterator);
	virtual status_t			OpenBPlusTreeFailed(Inode* inode);
	virtual status_t			TreeIterationFailed(status_t reason,
									Inode* parent);

private:
			status_t			_ResizeVolume();

			status_t			_UpdateIndexReferences(Transaction& transaction,
									Inode* inode, off_t newInodeID);
			status_t			_UpdateParent(Transaction& transaction,
									Inode* inode, off_t newInodeID);
			status_t			_MoveInode(Inode* inode, off_t& newInodeID);

			status_t			_TemporaryGetDiskSize(off_t& size);

private:
			bool				fShrinking;

			off_t				fBeginBlock;
			off_t				fEndBlock;

			off_t				fBitmapBlocks;

			block_run			fNewLog;

			bool				fForce;
};


#endif	// RESIZE_VISITOR_H
