/* 
** Copyright 2004, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
** Distributed under the terms of the OpenBeOS License.
*/


#include "DataEditor.h"

#include <Autolock.h>
#include <NodeMonitor.h>


class DataChange {
	public:
		virtual ~DataChange();

		virtual void Apply(off_t offset, uint8 *buffer, size_t size) = 0;
		virtual void Revert(off_t offset, uint8 *buffer, size_t size) = 0;
};

class ReplaceChange : public DataChange {
	public:
		ReplaceChange(off_t bufferOffset, uint8 *buffer,
			off_t offset, const uint8 *data, size_t size);
		~ReplaceChange();

		virtual void Apply(off_t offset, uint8 *buffer, size_t size);
		virtual void Revert(off_t offset, uint8 *buffer, size_t size);
	
	private:
		void Normalize(off_t bufferOffset, size_t bufferSize,
			off_t &offset, size_t &dataOffset, size_t &size);

		uint8 	*fNewData;
		uint8 	*fOldData;
		size_t	fSize;
		off_t	fOffset;
};


DataChange::~DataChange()
{
}


//	#pragma mark -


ReplaceChange::ReplaceChange(off_t bufferOffset, uint8 *buffer,
	off_t offset, const uint8 *data, size_t size)
{
	fOffset = offset;

	fNewData = (uint8 *)malloc(size);
	fOldData = (uint8 *)malloc(size);
	if (fNewData != NULL && fOldData != NULL) {
		// Save old data, and replace the data in the supplied buffer.
		// Note, the buffer must be large enough to hold the changes
		// at this point!
		memcpy(fNewData, data, size);
		memcpy(fOldData, buffer + offset - bufferOffset, size);
		memcpy(buffer + offset - bufferOffset, fNewData, size);

		fSize = size;		
	} else
		fSize = 0;
}


ReplaceChange::~ReplaceChange()
{
	free(fNewData);
	free(fOldData);
}


/** Normalizes the supplied offset & size pair to be limited by
 *	the buffer offset and size.
 *	All parameters must have been initialized before calling this
 *	method.
 */

void 
ReplaceChange::Normalize(off_t bufferOffset, size_t bufferSize, off_t &offset,
	size_t &dataOffset, size_t &size)
{
	if (fOffset < bufferOffset) {
		offset = bufferOffset;
		dataOffset = bufferOffset - fOffset;
		size -= dataOffset;
	}

	if (offset + size > bufferOffset + bufferSize)
		size = offset - bufferOffset + offset;
}


void 
ReplaceChange::Apply(off_t bufferOffset, uint8 *buffer, size_t bufferSize)
{
	// is it in our range?
	if (fOffset - bufferOffset > bufferSize || fOffset + fSize < bufferOffset)
		return;

	// don't change anything outside the supplied buffer
	off_t offset = fOffset;
	size_t dataOffset = 0;
	size_t size = fSize;
	Normalize(bufferOffset, bufferSize, offset, dataOffset, size);
	if (size == 0)
		return;

	// now we can safely change the buffer!
	memcpy(buffer + offset - bufferOffset, fNewData + dataOffset, size);
}


void 
ReplaceChange::Revert(off_t bufferOffset, uint8 *buffer, size_t bufferSize)
{
	// is it in our range?
	if (fOffset - bufferOffset > bufferSize || fOffset + fSize < bufferOffset)
		return;

	// don't change anything outside the supplied buffer
	off_t offset = fOffset;
	size_t dataOffset = 0;
	size_t size = fSize;
	Normalize(bufferOffset, bufferSize, offset, dataOffset, size);
	if (size == 0)
		return;

	// now we can safely revert the buffer!
	memcpy(buffer + offset - bufferOffset, fOldData + dataOffset, size);
}


//	#pragma mark -


DataEditor::DataEditor()
	:
	fLock("data view")
{
}


DataEditor::DataEditor(entry_ref &ref, const char *attribute)
	:
	fLock("data view")
{
	SetTo(ref, attribute);
}


DataEditor::DataEditor(BEntry &entry, const char *attribute)
	:
	fLock("data view")
{
	SetTo(entry, attribute);
}


DataEditor::DataEditor(const DataEditor &editor)
	:
	fLock("data view")
{
}


DataEditor::~DataEditor()
{
}


status_t 
DataEditor::SetTo(const char *path, const char *attribute)
{
	BEntry entry(path);
	SetTo(entry, attribute);
}


status_t 
DataEditor::SetTo(entry_ref &ref, const char *attribute)
{
	BEntry entry(&ref);
	SetTo(entry, attribute);
}


status_t 
DataEditor::SetTo(BEntry &entry, const char *attribute)
{
	status_t status = fFile.SetTo(&entry, B_READ_WRITE);
	if (status < B_OK) {
		// try to open read only
		status = fFile.SetTo(&entry, B_READ_ONLY);
		if (status < B_OK)
			return status;

		fIsReadOnly = true;
	}

	status = fFile.GetSize(&fSize);
	if (status < B_OK) {
		fFile.Unset();
		return status;
	}

	fBlockSize = 512;
	fAttribute = attribute;
	fRealViewOffset = -1;
	fViewOffset = 0;
	fRealViewSize = fViewSize = fBlockSize;
	fNeedsUpdate = true;
}


status_t 
DataEditor::SetToAttribute(const char *attribute)
{
	status_t status = InitCheck();
	if (status < B_OK)
		return status;

	fAttribute = attribute;

	// ToDo: attributes are not yet supported
	return B_ERROR;
}


status_t 
DataEditor::InitCheck()
{
	if (fAttribute != NULL)
		// ToDo: for now!
		return B_ERROR;

	return fFile.InitCheck();
}


void 
DataEditor::AddChange(DataChange *change)
{
	if (change == NULL)
		return;

	fChanges.AddItem(change);
	fLastChange = change;
}


status_t 
DataEditor::Replace(off_t offset, const uint8 *data, size_t length)
{
	if (!fLock.IsLocked())
		debugger("DataEditor: view not locked");

	if (fNeedsUpdate) {
		status_t status = Update();
		if (status < B_OK)
			return status;
	}

	ReplaceChange *change = new ReplaceChange(fRealViewOffset, fView, offset, data, length);
	AddChange(change);

	return B_OK;
}


status_t 
DataEditor::Remove(off_t offset, off_t length)
{
	if (!fLock.IsLocked())
		debugger("DataEditor: view not locked");

	// not yet implemented

	return B_ERROR;
}


status_t 
DataEditor::Insert(off_t offset, const uint8 *text, size_t length)
{
	if (!fLock.IsLocked())
		debugger("DataEditor: view not locked");

	// not yet implemented

	return B_ERROR;
}


void 
DataEditor::ApplyChanges()
{
	int32 count = fChanges.CountItems();

	for (int32 i = 0; i < count; i++) {
		DataChange *change = fChanges.ItemAt(i);
		change->Apply(fRealViewOffset, fView, fRealViewSize);
	}
}


void 
DataEditor::RemoveRedos()
{
}


status_t 
DataEditor::Undo()
{
	if (!CanUndo())
		return B_ERROR;

	int32 index = fChanges.IndexOf(fLastChange);
	fLastChange->Revert(fRealViewOffset, fView, fRealViewSize);

	if (index > 0)
		fLastChange = fChanges.ItemAt(index - 1);
	else
		fLastChange = NULL;

	return B_OK;
}


status_t 
DataEditor::Redo()
{
}


bool 
DataEditor::CanUndo() const
{
	return fLastChange != NULL;
}


bool 
DataEditor::CanRedo() const
{
	return fLastChange != NULL && fChanges.IndexOf(fLastChange) < fChanges.CountItems() - 1;
}


status_t 
DataEditor::SetFileSize(off_t size)
{
	fSize = size;
}


status_t
DataEditor::SetViewOffset(off_t offset)
{
	if (fView == NULL) {
		status_t status = SetViewSize(fViewSize);
		if (status < B_OK)
			return status;
	}

	fRealViewOffset = (offset / fBlockSize) * fBlockSize;
	fViewOffset = offset;
	fNeedsUpdate = true;
}


status_t 
DataEditor::SetViewSize(size_t size)
{
	size_t realSize = (size + fBlockSize - 1) & ~(fBlockSize - 1);
		// round to the next multiple of block size

	if (realSize == fRealViewSize && fView != NULL) {
		fViewSize = size;
		return B_OK;
	}

	uint8 *view = (uint8 *)realloc(fView, realSize);
	if (view == NULL)
		return B_NO_MEMORY;

	fView = view;
	fRealViewSize = realSize;
	fViewSize = size;
	fNeedsUpdate = true;

	return B_OK;
}


void 
DataEditor::SetBlockSize(size_t size)
{
	fBlockSize = size;
}


status_t 
DataEditor::Update()
{
	ssize_t bytesRead = fFile.ReadAt(fRealViewOffset, fView, fRealViewSize);
	if (bytesRead < B_OK)
		return bytesRead;

	ApplyChanges();
	fNeedsUpdate = false;

	return B_OK;
}


status_t
DataEditor::GetViewBuffer(const uint8 **_buffer)
{
	if (!fLock.IsLocked())
		debugger("DataEditor: view not locked");

	status_t status = B_OK;

	if (fView == NULL)
		status = SetViewOffset(fViewOffset);

	if (status == B_OK && fNeedsUpdate)
		status = Update();

	if (status < B_OK)
		return status;

	*_buffer = fView + fViewOffset - fRealViewOffset;
	return B_OK;
}


bool 
DataEditor::Lock()
{
	return fLock.Lock();
}


void 
DataEditor::Unlock()
{
	fLock.Unlock();
}


status_t 
DataEditor::StartWatching(BMessenger target)
{
	node_ref node;
	status_t status = fFile.GetNodeRef(&node);
	if (status < B_OK)
		return status;

	return watch_node(&node, B_WATCH_STAT | B_WATCH_ATTR, target);
}


void 
DataEditor::StopWatching(BMessenger target)
{
	stop_watching(target);
}

