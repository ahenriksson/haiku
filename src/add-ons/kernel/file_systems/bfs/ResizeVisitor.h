/*
 * Copyright 2012, Andreas Henriksson, sausageboy@gmail.com
 * This file may be used under the terms of the MIT License.
 */
#ifndef RESIZE_VISITOR_H
#define RESIZE_VISITOR_H


#include "system_dependencies.h"

#include "bfs_control.h"
#include "FileSystemVisitor.h"


class Inode;
class Transaction;


class ResizeVisitor : public FileSystemVisitor {
public:
								ResizeVisitor(Volume* volume);

	void						StartResize();
	void						FinishResize();

	resize_control&				Control() { return fControl; }

	virtual status_t			VisitInode(Inode* inode, const char* treeName);

	virtual status_t			OpenInodeFailed(status_t reason, ino_t id,
									Inode* parent, char* treeName,
									TreeIterator* iterator);
	virtual status_t			OpenBPlusTreeFailed(Inode* inode);
	virtual status_t			TreeIterationFailed(status_t reason,
									Inode* parent);

private:
			status_t			_ResizeVolume();

			// moving the inode
			status_t			_UpdateParent(Transaction& transaction,
									Inode* inode, off_t newInodeID,
									const char* treeName);
			status_t			_UpdateAttributeDirectory(
									Transaction& transaction, Inode* inode,
									block_run newInodeRun);
			status_t			_UpdateIndexReferences(Transaction& transaction,
									Inode* inode, off_t newInodeID,
									bool rootOrIndexDir);
			status_t			_UpdateTree(Transaction& transaction,
									Inode* inode, off_t newInodeID);
			status_t			_UpdateChildren(Transaction& transaction,
									Inode* inode, off_t newInodeID);
			status_t			_UpdateSuperBlock(Inode* inode,
									off_t newInodeID);
			status_t			_MoveInode(Inode* inode, off_t& newInodeID,
									const char* treeName);

			bool				_ControlValid();

			void				_SetError(status_t status,
									uint32 failurePoint = BFS_OTHER_ERROR);

private:
			resize_control		fControl;
			bool				fShrinking;

			off_t				fNumBlocks;

			off_t				fBeginBlock;
			off_t				fEndBlock;

			off_t				fBitmapBlocks;

			block_run			fNewLog;
};


#endif	// RESIZE_VISITOR_H
