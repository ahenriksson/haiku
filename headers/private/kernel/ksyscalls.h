/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_SYSCALLS_H
#define _KERNEL_SYSCALLS_H

#include <sys/types.h>


enum {
	SYSCALL_NULL = 0,				/*  0 */
	SYSCALL_MOUNT,
	SYSCALL_UNMOUNT,
	SYSCALL_SYNC,
	SYSCALL_OPEN,
	SYSCALL_CLOSE,					/*  5 */
	SYSCALL_FSYNC,
	SYSCALL_READ,
	SYSCALL_WRITE,
	SYSCALL_SEEK,
	SYSCALL_IOCTL,					/* 10 */
	SYSCALL_CREATE,
	SYSCALL_UNLINK,
	SYSCALL_RENAME,
	SYSCALL_READ_PATH_STAT,
	SYSCALL_WRITE_PATH_STAT,		/* 15 */
	SYSCALL_SYSTEM_TIME,
	SYSCALL_SNOOZE_ETC,
	SYSCALL_SEM_CREATE,
	SYSCALL_SEM_DELETE,
	SYSCALL_SEM_ACQUIRE,			/* 20 */
	SYSCALL_SEM_ACQUIRE_ETC,
	SYSCALL_SEM_RELEASE,
	SYSCALL_SEM_RELEASE_ETC,
	SYSCALL_FIND_THREAD,
	SYSCALL_EXIT_THREAD,			/* 25 */
	SYSCALL_CREATE_TEAM,
	SYSCALL_WAIT_FOR_THREAD,
	SYSCALL_WAIT_FOR_TEAM,
	SYSCALL_CREATE_AREA,
	SYSCALL_CLONE_AREA,				/* 30 */
	SYSCALL_VM_MAP_FILE,
	SYSCALL_DELETE_AREA,
	SYSCALL_GET_AREA_INFO,
	SYSCALL_FIND_AREA,
	SYSCALL_SPAWN_THREAD,			/* 45 */
	SYSCALL_KILL_THREAD,
	SYSCALL_SUSPEND_THREAD,
	SYSCALL_RESUME_THREAD,
	SYSCALL_KILL_TEAM,
	SYSCALL_GET_CURRENT_TEAM_ID,	/* 40 */
	SYSCALL_GETCWD,
	SYSCALL_SETCWD,
	SYSCALL_PORT_CREATE,
	SYSCALL_PORT_CLOSE,
	SYSCALL_PORT_DELETE,			/* 45 */
	SYSCALL_PORT_FIND,
	SYSCALL_PORT_GET_INFO,
	SYSCALL_PORT_GET_NEXT_PORT_INFO,
	SYSCALL_PORT_BUFFER_SIZE,
	SYSCALL_PORT_BUFFER_SIZE_ETC,	/* 50 */
	SYSCALL_PORT_COUNT,
	SYSCALL_PORT_READ,
	SYSCALL_PORT_READ_ETC,
	SYSCALL_PORT_SET_OWNER,
	SYSCALL_PORT_WRITE,				/* 55 */
	SYSCALL_PORT_WRITE_ETC,
	SYSCALL_SEM_GET_COUNT,
	SYSCALL_SEM_GET_SEM_INFO,
	SYSCALL_SEM_GET_NEXT_SEM_INFO,
	SYSCALL_SEM_SET_SEM_OWNER,		/* 60 */
	SYSCALL_FDDUP,
	SYSCALL_FDDUP2,
	SYSCALL_GET_PROC_TABLE,
	SYSCALL_GETRLIMIT,
	SYSCALL_SETRLIMIT,				/* 65 */
	SYSCALL_READ_FS_INFO,
	SYSCALL_WRITE_FS_INFO,
	SYSCALL_NEXT_DEVICE,
	SYSCALL_unused_69,
	SYSCALL_unused_70,				/* 70 */
	SYSCALL_SYSCTL,
	SYSCALL_SOCKET,
	SYSCALL_ACCESS,
	SYSCALL_READ_STAT,
	SYSCALL_READ_DIR,				/* 75 */
	SYSCALL_REWIND_DIR,
	SYSCALL_OPEN_DIR,
	SYSCALL_CREATE_DIR,
	SYSCALL_SETENV,
	SYSCALL_GETENV, 				/* 80 */
	SYSCALL_OPEN_ENTRY_REF,
	SYSCALL_OPEN_DIR_ENTRY_REF,
	SYSCALL_OPEN_DIR_NODE_REF,
	SYSCALL_CREATE_ENTRY_REF,
	SYSCALL_CREATE_DIR_ENTRY_REF,	/* 85 */
	SYSCALL_CREATE_SYMLINK,
	SYSCALL_READ_LINK,
	SYSCALL_GET_THREAD_INFO,
	SYSCALL_GET_NEXT_THREAD_INFO,
	SYSCALL_GET_TEAM_INFO,			/* 90 */
	SYSCALL_GET_NEXT_TEAM_INFO,
	SYSCALL_CREATE_LINK,
	SYSCALL_REMOVE_DIR,
	SYSCALL_SEND_DATA,
	SYSCALL_RECEIVE_DATA,			/* 95 */
	SYSCALL_HAS_DATA,
	SYSCALL_OPEN_ATTR_DIR,
	SYSCALL_CREATE_ATTR,
	SYSCALL_OPEN_ATTR,
	SYSCALL_WRITE_STAT,				/* 100 */
	SYSCALL_REMOVE_ATTR,
	SYSCALL_RENAME_ATTR,
	SYSCALL_RETURN_FROM_SIGNAL,
	SYSCALL_unused_104,
	SYSCALL_SIGACTION,				/* 105 */
	SYSCALL_OPEN_INDEX_DIR,
	SYSCALL_CREATE_INDEX,
	SYSCALL_READ_INDEX_STAT,
	SYSCALL_REMOVE_INDEX,
	SYSCALL_SEND_SIGNAL,			/* 110 */
	SYSCALL_SET_ALARM,
	SYSCALL_SELECT,
	SYSCALL_POLL,
	SYSCALL_REGISTER_IMAGE,
	SYSCALL_UNREGISTER_IMAGE,		/* 115 */
	SYSCALL_GET_IMAGE_INFO,
	SYSCALL_GET_NEXT_IMAGE_INFO,
	SYSCALL_START_WATCHING,
	SYSCALL_STOP_WATCHING,
	SYSCALL_STOP_NOTIFYING,			/* 120 */
	SYSCALL_SET_THREAD_PRIORITY,
	SYSCALL_GET_NEXT_AREA_INFO,
	SYSCALL_SET_AREA_PROTECTION,
	SYSCALL_RESIZE_AREA,
	SYSCALL_AREA_FOR,				/* 125 */
	SYSCALL_ATOMIC_SET,
	SYSCALL_ATOMIC_TEST_AND_SET,
	SYSCALL_ATOMIC_ADD,
	SYSCALL_ATOMIC_AND,
	SYSCALL_ATOMIC_OR,				/* 130 */
	SYSCALL_ATOMIC_GET,
	SYSCALL_ATOMIC_SET64,
	SYSCALL_ATOMIC_TEST_AND_SET64,
	SYSCALL_ATOMIC_ADD64,
	SYSCALL_ATOMIC_AND64,			/* 135 */
	SYSCALL_ATOMIC_OR64,
	SYSCALL_ATOMIC_GET64,
	SYSCALL_SET_REAL_TIME_CLOCK,
	SYSCALL_DEBUG_OUTPUT,
	SYSCALL_GET_SYSTEM_INFO,		/* 140 */
	SYSCALL_RENAME_THREAD,
	SYSCALL_DIR_NODE_REF_TO_PATH,
};

int syscall_dispatcher(unsigned long call_num, void *arg_buffer, uint64 *call_ret);

#endif	/* _KERNEL_SYSCALLS_H */
