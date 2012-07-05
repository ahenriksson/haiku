#include "FileSystemVisitor.h"

#include "BPlusTree.h"
#include "Debug.h"
#include "Inode.h"
#include "Volume.h"


FileSystemVisitor::FileSystemVisitor(Volume* volume)
	:
	fVolume(volume),
	fIterator(NULL)
{
}


FileSystemVisitor::~FileSystemVisitor()
{
	Stop();
}


//	#pragma mark - file system traversal


/*! Visit the next inode.

	\return
	- \c B_ENTRY_NOT_FOUND : All nodes specified in Start() have been
	  traversed
	- Any error code returned by the overridable functions
*/
status_t
FileSystemVisitor::Next()
{
	status_t status;

	while (true) {
		const char* name = NULL;
		Inode* inode;

		if (fIterator == NULL) {
			if (!fStack.Pop(&fCurrent)) {
				// we're done
				return B_ENTRY_NOT_FOUND;
			}

			// open inode
			Vnode vnode(fVolume, fCurrent);
			status = vnode.Get(&inode);
			if (status != B_OK) {
				status = OpenInodeFailed(status, fVolume->ToBlock(fCurrent),
					NULL, NULL, NULL);
				if (status == B_OK)
					continue;
				return status;
			}

			if (inode->IsContainer()) {
				// open directory
				BPlusTree* tree = inode->Tree();
				if (tree == NULL) {
					status = OpenBPlusTreeFailed(inode);
					if (status == B_OK)
						continue;
					return status;
				}

				fParent = inode;

				// get iterator for the next directory
				fIterator = new(std::nothrow) TreeIterator(tree);
				if (fIterator == NULL)
					RETURN_ERROR(B_NO_MEMORY);

				// the inode must stay locked in memory until the iterator
				// is freed
				vnode.Keep();
			}
		} else {
			char treeName[B_FILE_NAME_LENGTH];
			uint16 length;
			ino_t id;

			status_t status = fIterator->GetNextEntry(treeName, &length,
				B_FILE_NAME_LENGTH, &id);
			if (status != B_OK) {
				// we no longer need this iterator
				delete fIterator;
				fIterator = NULL;

				// unlock the directory's inode from memory
				put_vnode(fVolume->FSVolume(),
					fVolume->ToVnode(fCurrent));

				if (status == B_ENTRY_NOT_FOUND) {
					// We iterated over all entries already, just go on
					// to the next
					continue;
				}

				// iterating over the B+tree failed
				//return TreeIterationFailed(status, fParent);
				status = TreeIterationFailed(status, fParent);
				if (status == B_OK)
					continue;
				return status;
			}

			// ignore "." and ".." entries
			if (!strcmp(treeName, ".") || !strcmp(treeName, ".."))
				continue;

			Vnode vnode(fVolume, id);
			status = vnode.Get(&inode);
			if (status != B_OK) {
				status = OpenInodeFailed(status, id, fParent, treeName,
					fIterator);
				if (status == B_OK)
					continue;
				return status;
			}

			status = VisitDirectoryEntry(inode, fParent, treeName);
			if (status != B_OK)
				return status;

			if (inode->IsContainer() && !inode->IsIndex()) {
				// push the directory on the stack, it will be visited after
				// its children
				fStack.Push(inode->BlockRun());
				continue;
			}

			name = treeName;
		}

		// If the inode has an attribute directory that we want to visit,
		// push it on the stack
		if ((fFlags & VISIT_ATTRIBUTE_DIRECTORIES)
			&& !inode->Attributes().IsZero()) {
			fStack.Push(inode->Attributes());
		}

		return VisitInode(inode, name);
	}
	// is never reached
}


/*! Start/restart traversal. \a flags is used to specify the nodes visited:
	- \c VISIT_REGULAR :	Visit the nodes that are reachable by traversing
							the file system from the root directory.
	- \c VISIT_INDICES :	Visit the index directory and indices
	- \c VISIT_REMOVED :	Visit removed vnodes
	- \c VISIT_ATTRIBUTE_DIRECTORIES :	Visit the attribute directory and
										attributes of files that have them.
*/
void
FileSystemVisitor::Start(uint32 flags)
{
	// initialize state
	Stop();
	fCurrent.SetTo(0, 0, 0);
	fParent = NULL;
	fStack.MakeEmpty();

	fFlags = flags;

	if (fFlags & VISIT_REGULAR)
		fStack.Push(fVolume->Root());

	if (fFlags & VISIT_INDICES)
		fStack.Push(fVolume->Indices());

	if (fFlags & VISIT_REMOVED) {
		// Put removed vnodes to the stack -- they are not reachable by
		// traversing the file system anymore.
		InodeList::Iterator iterator = fVolume->RemovedInodes().GetIterator();
		while (Inode* inode = iterator.Next()) {
			fStack.Push(inode->BlockRun());
		}
	}
}


/*! Free aquired resources. This function is called from the destructor.
*/
void
FileSystemVisitor::Stop()
{
	if (fIterator != NULL) {
		delete fIterator;
		fIterator = NULL;

		// the current directory inode is still locked in memory
		put_vnode(fVolume->FSVolume(), fVolume->ToVnode(fCurrent));
	}
}


//	#pragma mark - overrideable actions


/*! Called when an inode is opened while iterating through its parent
	directory. Note that this function is not called for inodes which
	don't have a proper parent directory, namely:
	- The root directory
	- The index directory
	- Attribute directories
	- Removed nodes

	Return \c B_OK to continue traversing, any other error code to stop
	traversal and propagate the error to the caller of Next(). In the
	latter case, VisitInode() will never be called for this inode.
*/
status_t
FileSystemVisitor::VisitDirectoryEntry(Inode* inode, Inode* parent,
	const char* treeName)
{
	return B_OK;
}


/*! Called for every inode, some time after VisitDirectoryEntry(). For
	directories, all subdirectories will be traversed before this
	function is called.

	Unless traversal has been stopped by an error handling function, all
	calls to Next() end by invoking this function, and the return value
	is passed along to the caller.
*/
status_t
FileSystemVisitor::VisitInode(Inode* inode, const char* treeName)
{
	return B_OK;
}


/*! Called when opening an inode fails. If the failure happened while
	iterating through a directory, \a parent, \a treeName and \a iterator
	will be provided. Otherwise, they will be \c NULL.

	\return
	- \c B_OK : The traversal continues to the next inode
	- Other : The traversal stops and the error code is propagated to the
	  caller of Next().
*/
status_t
FileSystemVisitor::OpenInodeFailed(status_t reason, ino_t id, Inode* parent,
	char* treeName, TreeIterator* iterator)
{
	return B_OK;
}


/*! Called when opening a b+tree fails.
	
	\return
	- \c B_OK : The traversal continues to the next inode
	- Other : The traversal stops and the error code is propagated to the
	  caller of Next().
*/
status_t
FileSystemVisitor::OpenBPlusTreeFailed(Inode* inode)
{
	return B_OK;
}


/*! Called if we failed to get the next node while iterating a container.
	
	\return
	- \c B_OK : The traversal continues to the next inode
	- Other : The traversal stops and the error code is propagated to the
	  caller of Next().
*/
status_t
FileSystemVisitor::TreeIterationFailed(status_t reason, Inode* parent)
{
	return B_OK;
}
