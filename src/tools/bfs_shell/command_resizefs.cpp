/*
 * Copyright 2012, Andreas Henriksson, sausageboy@gmail.com.
 * Distributed under the terms of the MIT License.
 */


#include "fssh_stdio.h"
#include "syscalls.h"

#include "bfs.h"
#include "bfs_control.h"


namespace FSShell {


void Usage(const char* name);

void PrintInformation(const resize_control& result);
void PrintError(const resize_control& result);
void PrintStats(const resize_control& result);
const char* FailureToString(uint32 failurePoint);


fssh_status_t
command_resizefs(int argc, const char* const* argv)
{
	// get command line options
	bool hasNewSize = false;
	uint64 newSize = 0;

	bool check = false;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--check"))
			check = true;
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			Usage(argv[0]);
			return B_OK;
		} else {
			if (fssh_sscanf(argv[i], "%" B_SCNu64, &newSize) < 1) {
				fssh_dprintf("Unknown argument or invalid size\n\n");
				Usage(argv[0]);
				return B_ERROR;
			}
			hasNewSize = true;
		}
	}
	
	if (!hasNewSize) {
		fssh_dprintf("No size specified\n\n");
		Usage(argv[0]);
		return B_ERROR;
	}

	int rootDir = _kern_open_dir(-1, "/myfs");
	if (rootDir < 0) {
		fssh_dprintf("Error: Couldn't open root directory\n");
		return rootDir;
	}

	struct resize_control result;
	memset(&result, 0, sizeof(result));
	result.magic = BFS_IOCTL_RESIZE_MAGIC;

	if (check)
		result.flags |= BFS_CHECK_RESIZE;

	result.new_size = newSize;
	
	// start resizing
	fssh_status_t status = _kern_ioctl(rootDir, BFS_IOCTL_START_RESIZE,
		&result, sizeof(result));

	if (status != B_OK) {
		fssh_dprintf("Error: Couldn't perform 'start' ioctl\n");
	    return status;
	}

	PrintInformation(result);
	if (result.status != B_OK) {
		PrintError(result);
		_kern_close(rootDir);
		return result.status;
	}

	if (check) {
		_kern_close(rootDir);
		fssh_dprintf("Resizing appears to be possible.\n");
		return B_OK;
	}

	// move all files out of the way
	while (true) {
		status = _kern_ioctl(rootDir, BFS_IOCTL_MOVE_NEXT_NODE, &result,
			sizeof(result));
		if (status == B_ENTRY_NOT_FOUND)
			break;

		// we couldn't return the resizefs_control struct
		if (status != B_OK) {
			fssh_dprintf("Error: Couldn't perform 'next' ioctl\n");
			break;
		}

		if (result.status != B_OK) {
			PrintError(result);
			status = result.status;
			break;
		}
	}

	// finish the resize
	if (_kern_ioctl(rootDir, BFS_IOCTL_FINISH_RESIZE, &result, sizeof(result))
			!= B_OK) {
		_kern_close(rootDir);
		fssh_dprintf("Error: Couldn't perform 'finish' ioctl\n");
		return errno;
	}

	_kern_close(rootDir);

	PrintStats(result);

	if (result.status != B_OK)
		PrintError(result);
	else
		fssh_dprintf("File system successfully resized!\n");

	return result.status;
}


void
Usage(const char* name)
{
	fssh_dprintf("Usage: %s <new size> [options]\n"
		"\t-c, --check   Don't resize, just check if it would be possible\n"
		"\t-h, --help    Print this message\n", name);
}


void
PrintInformation(const resize_control& result)
{
	fssh_dprintf("New file system information:\n");
	fssh_dprintf("\tBitmap:     %" B_PRIu16 " blocks (was %" B_PRIu16 ")\n",
		result.stats.bitmap_blocks_new, result.stats.bitmap_blocks_old);
	fssh_dprintf("\tLog start:  block %" B_PRIu16 " (was %" B_PRIu16 ")\n",
		result.stats.log_start_new, result.stats.log_start_old);
	fssh_dprintf("\tLog length: %" B_PRIu16 " blocks (was %" B_PRIu16 ")\n",
		result.stats.log_length_new, result.stats.log_length_old);
	fssh_dprintf("\tBlock size: %" B_PRIu16 " bytes\n\n",
		result.stats.block_size);
}


void
PrintError(const resize_control& result)
{
	fssh_dprintf("Error:  %s.\n", FailureToString(result.failure_point));
	fssh_dprintf("Status: %s\n", fssh_strerror(result.status));

	if ((result.failure_point == BFS_MOVE_INODE_FAILED)
		|| (result.failure_point == BFS_MOVE_STREAM_FAILED)) {
		fssh_dprintf("\tIn inode %" B_PRIdINO ", \"%s\"\n", result.inode,
			result.name);
	}

	fssh_dprintf("\n");
}


void
PrintStats(const resize_control& result)
{
	fssh_dprintf("\tInodes moved:       %" B_PRIu64 "\n",
		result.stats.inodes_moved);
	fssh_dprintf("\tData streams moved: %" B_PRIu64 "\n\n",
		result.stats.streams_moved);
}


const char*
FailureToString(uint32 failurePoint)
{
	switch (failurePoint) {
		case BFS_OTHER_ERROR:
			return "Other error";
		case BFS_SIZE_TOO_LARGE:
			return "Can't grow disk this much";
		case BFS_DISK_TOO_SMALL:
			return "Not enough room on device to grow file system";
		case BFS_NO_SPACE:
			return "Not enough space left";
		case BFS_MOVE_LOG_FAILED:
			return "Failed to move the file system log";
		case BFS_MOVE_INODE_FAILED:
			return "Failed to move inode";
		case BFS_MOVE_STREAM_FAILED:
			return "Failed to move file data";
		case BFS_CHANGE_SIZE_FAILED:
			return "Failed to update file system size";
	}
	return "(Can't get type of failure)";
}


}	// namespace FSShell
