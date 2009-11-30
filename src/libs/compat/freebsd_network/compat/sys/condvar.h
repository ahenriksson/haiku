/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_CONDVAR_H_
#define _FBSD_COMPAT_SYS_CONDVAR_H_


#include <sys/queue.h>


struct cv {
	int cv_waiters;
};


void cv_init(struct cv*, const char*);
void cv_wait(struct cv*, struct mtx*);
int cv_timedwait(struct cv*, struct mtx*, int);
void cv_signal(struct cv*);

#endif /* _FBSD_COMPAT_SYS_CONDVAR_H_ */
