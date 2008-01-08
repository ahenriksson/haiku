/*
 * Copyright 2006-2007 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		Ithamar R. Adema <ithamar@unet.nl>
 */
#include "PartitionList.h"
#include "Support.h"

#include <ColumnTypes.h>
#include <Path.h>


static const char* kUnavailableString = "";

enum {
//	kBitmapColumn,
	kDeviceColumn,
	kFilesystemColumn,
	kVolumeNameColumn,
	kMountedAtColumn,
	kSizeColumn
};


PartitionListRow::PartitionListRow(BPartition* partition)
	: Inherited()
	, fPartitionID(partition->ID())
{
//	SetField(new BBitmapField(NULL), kBitmapColumn);

	BPath path;
	partition->GetPath(&path);
	SetField(new BStringField(path.Path()), kDeviceColumn);

//	if (partition->ContainsPartitioningSystem()) {
//		SetField(new BStringField(partition->ContentType()), kFilesystemColumn);
//	} else {
//		SetField(new BStringField(kUnavailableString), kFilesystemColumn);
//	}

	if (partition->ContainsFileSystem()) {
		SetField(new BStringField(partition->ContentType()), kFilesystemColumn);
		SetField(new BStringField(partition->ContentName()), kVolumeNameColumn);
	} else {
		SetField(new BStringField(kUnavailableString), kFilesystemColumn);
		SetField(new BStringField(kUnavailableString), kVolumeNameColumn);
	}
	
	if (partition->IsMounted() && partition->GetMountPoint(&path) == B_OK) {
		SetField(new BStringField(path.Path()),  kMountedAtColumn);
	} else {
		SetField(new BStringField(kUnavailableString), kMountedAtColumn);
	}

	char size[1024];
	SetField(new BStringField(string_for_size(partition->Size(), size)), kSizeColumn);
}


PartitionListView::PartitionListView(const BRect& frame, uint32 resizeMode)
	: Inherited(frame, "storagelist", resizeMode, 0, B_NO_BORDER, true)
{
//	AddColumn(new BBitmapColumn("", 20, 20, 100, B_ALIGN_CENTER), kBitmapColumn);
	AddColumn(new BStringColumn("Device", 150, 50, 500, B_TRUNCATE_MIDDLE), kDeviceColumn);
	AddColumn(new BStringColumn("Filesystem", 100, 50, 500, B_TRUNCATE_MIDDLE), kFilesystemColumn);
	AddColumn(new BStringColumn("Volume Name", 130, 50, 500, B_TRUNCATE_MIDDLE), kVolumeNameColumn);
	AddColumn(new BStringColumn("Mounted At", 100, 50, 500, B_TRUNCATE_MIDDLE), kMountedAtColumn);
	AddColumn(new BStringColumn("Size", 100, 50, 500, B_TRUNCATE_END, B_ALIGN_RIGHT), kSizeColumn);
}


PartitionListRow*
PartitionListView::FindRow(partition_id id, PartitionListRow* parent)
{
	for (int32 i = 0; i < CountRows(parent); i++) {
		PartitionListRow* item = dynamic_cast<PartitionListRow*>(RowAt(i, parent));
		if (item != NULL && item->ID() == id)
			return item;
		if (CountRows(item) > 0) {
			// recurse into child rows
			item = FindRow(id, item);
			if (item)
				return item;
		}
	}

	return NULL;
}


PartitionListRow*
PartitionListView::AddPartition(BPartition* partition)
{
	PartitionListRow* partitionrow = FindRow(partition->ID());
	
	// Forget about it if this partition is already in the listview
	if (partitionrow != NULL) {
		return partitionrow;
	}
	
	// Create the row for this partition
	partitionrow = new PartitionListRow(partition);

	// If this partition has a parent...
	if (partition->Parent() != NULL) {
		// check if it is in the listview
		PartitionListRow* parent = FindRow(partition->Parent()->ID());
		// If parent of this partition is not yet in the list
		if (parent == NULL) {
			// add it
			parent = AddPartition(partition->Parent());
		}
		// Now it is ok to add this partition under its parent
		AddRow(partitionrow, parent);
	} else {
		// If this partition has no parent, add it in the 'root'
		AddRow(partitionrow);
	}
	
	return partitionrow;
}
