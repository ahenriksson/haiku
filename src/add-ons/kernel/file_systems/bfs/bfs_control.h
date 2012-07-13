/*
 * Copyright 2001-2012, Axel Dörfler, axeld@pinc-software.de
 * Copyright 2012, Andreas Henriksson, sausageboy@gmail.com
 * This file may be used under the terms of the MIT License.
 */
#ifndef BFS_CONTROL_H
#define BFS_CONTROL_H


//! additional functionality exported via ioctl()


#ifdef BFS_SHELL
#	include "system_dependencies.h"
#else
#	include <SupportDefs.h>
#endif


/* ioctl to check the version of BFS used - parameter is a uint32 *
 * where the number is stored
 */
#define BFS_IOCTL_VERSION			14200

#define BFS_IOCTL_UPDATE_BOOT_BLOCK	14204

struct update_boot_block {
	uint32			offset;
	const uint8*	data;
	uint32			length;
};


/* ioctls to use the "chkbfs" feature from the outside
 * all calls use a struct check_result as single parameter
 */
#define	BFS_IOCTL_START_CHECKING	14201
#define BFS_IOCTL_STOP_CHECKING		14202
#define BFS_IOCTL_CHECK_NEXT_NODE	14203

/* The "pass" field constants */
#define BFS_CHECK_PASS_BITMAP		0
#define BFS_CHECK_PASS_INDEX		1

/* All fields except "flags", and "name" must be set to zero before
 * BFS_IOCTL_START_CHECKING is called, and magic must be set.
 */
struct check_control {
	uint32		magic;
	uint32		pass;
	uint32		flags;
	char		name[B_FILE_NAME_LENGTH];
	ino_t		inode;
	uint32		mode;
	uint32		errors;
	struct {
		uint64	missing;
		uint64	already_set;
		uint64	freed;

		uint64	direct_block_runs;
		uint64	indirect_block_runs;
		uint64	indirect_array_blocks;
		uint64	double_indirect_block_runs;
		uint64	double_indirect_array_blocks;
		uint64	blocks_in_direct;
		uint64	blocks_in_indirect;
		uint64	blocks_in_double_indirect;
		uint64	partial_block_runs;
		uint32	block_size;
	} stats;
	status_t	status;
};

/* values for the flags field */
#define BFS_FIX_BITMAP_ERRORS	1
#define BFS_REMOVE_WRONG_TYPES	2
	/* files that shouldn't be part of its parent will be removed
	 * (i.e. a directory contains an attribute, ...)
	 * Works only if B_FIX_BITMAP_ERRORS is set, too
	 */
#define BFS_REMOVE_INVALID		4
	/* removes nodes that couldn't be opened at all from its parent
	 * directory.
	 * Also requires the B_FIX_BITMAP_ERRORS to be set.
	 */
#define BFS_FIX_NAME_MISMATCHES	8
#define BFS_FIX_BPLUSTREES		16

/* values for the errors field */
#define BFS_MISSING_BLOCKS		1
#define BFS_BLOCKS_ALREADY_SET	2
#define BFS_INVALID_BLOCK_RUN	4
#define BFS_COULD_NOT_OPEN		8
#define BFS_WRONG_TYPE			16
#define BFS_NAMES_DONT_MATCH	32
#define BFS_INVALID_BPLUSTREE	64

/* check control magic value */
#define BFS_IOCTL_CHECK_MAGIC	'BChk'


/* ioctls to resize the file system
 * usage is analogous to that of "chkbfs" above
 */
#define BFS_IOCTL_START_RESIZE		14205
#define BFS_IOCTL_FINISH_RESIZE		14206
#define BFS_IOCTL_MOVE_NEXT_NODE	14207

struct resize_control {
	uint32		magic;
	uint32		flags;
	uint64		new_size;
	char		name[B_FILE_NAME_LENGTH];
	ino_t		inode;
	uint32		failure_point;
	struct {
		uint64	inodes_moved;
		uint64	streams_moved;

		uint16	bitmap_blocks_new;
		uint16	log_start_new;
		uint16	log_length_new;

		uint16	bitmap_blocks_old;
		uint16	log_start_old;
		uint16	log_length_old;

		uint32	block_size;
	} stats;
	status_t	status;
};

/* resize flags */
#define BFS_CHECK_RESIZE		1

/* resize points of failure */
#define BFS_OTHER_ERROR			1
#define BFS_SIZE_TOO_LARGE		2
#define BFS_DISK_TOO_SMALL		3
#define BFS_NO_SPACE			4
#define BFS_MOVE_LOG_FAILED		5
#define BFS_MOVE_INODE_FAILED	6
#define BFS_MOVE_STREAM_FAILED	7
#define BFS_CHANGE_SIZE_FAILED	8

/* magic resize constant */
#define BFS_IOCTL_RESIZE_MAGIC	'BRsz'


#endif	/* BFS_CONTROL_H */
