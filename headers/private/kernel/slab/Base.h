/*
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *      Hugo Santos, hugosantos@gmail.com
 */

#ifndef _SLAB_BASE_SLAB_H_
#define _SLAB_BASE_SLAB_H_

#include <stdint.h>

#include <KernelExport.h>
#include <OS.h>

#include <lock.h>
#include <vm_low_memory.h>
#include <util/AutoLock.h>
#include <util/list.h>

#ifdef __cplusplus
#include <utility> // pair<>

extern "C" {
#endif

enum {
	CACHE_DONT_SLEEP		= 1 << 0,
	CACHE_ALIGN_TO_TOTAL	= 1 << 16,
};

static const int kMinimumSlabItems = 32;

typedef status_t (*base_cache_constructor)(void *cookie, void *object);
typedef void (*base_cache_destructor)(void *cookie, void *object);

/* base Slab implementation, opaque to the backend used.
 *
 * NOTE: the caller is responsible for the Cache's locking.
 *       Cache<> below handles it as well. */

typedef struct base_cache {
	char name[32];
	size_t object_size;
	size_t cache_color_cycle;
	struct list empty, partial, full;
	size_t empty_count, pressure;
	base_cache_constructor constructor;
	base_cache_destructor destructor;
	void *cookie;
} base_cache;

typedef struct cache_slab {
	void *pages;
	size_t count, size;
	size_t offset;
	struct cache_object_link *free;
	struct list_link link;
} cache_slab;

// TODO add reclaim method to base_cache to be called under severe memory
//		pressure so the slab owner can free as much buffers as possible.

void base_cache_init(base_cache *cache, const char *name, size_t object_size,
	size_t alignment, base_cache_constructor constructor,
	base_cache_destructor destructor, void *cookie);
void base_cache_destroy(base_cache *cache,
	void (*return_slab)(base_cache *, cache_slab *));
void base_cache_low_memory(base_cache *cache, int32 level,
	void (*return_slab)(base_cache *, cache_slab *));

void *base_cache_allocate_object(base_cache *cache);
void *base_cache_allocate_object_with_new_slab(base_cache *cache,
	cache_slab *slab);
int base_cache_return_object(base_cache *cache, cache_slab *slab,
	void *object);

typedef status_t (*base_cache_owner_prepare)(void *parent,
	cache_slab *slab, void *object);
typedef void (*base_cache_owner_unprepare)(void *parent, cache_slab *slab,
	void *object);

cache_slab *base_cache_construct_slab(base_cache *cache, cache_slab *slab,
	void *pages, size_t byte_count, void *parent,
	base_cache_owner_prepare prepare, base_cache_owner_unprepare unprepare);
void base_cache_destruct_slab(base_cache *cache, cache_slab *slab,
	void *parent, base_cache_owner_unprepare unprepare);

#ifdef __cplusplus
}

// Slab implementation, glues together the frontend, backend as
// well as the Slab strategy used.
template<typename Strategy>
class Cache : protected base_cache {
public:
	typedef Cache<Strategy>			ThisCache;
	typedef base_cache_constructor	Constructor;
	typedef base_cache_destructor	Destructor;

	Cache(const char *name, size_t objectSize, size_t alignment,
		Constructor constructor, Destructor destructor, void *cookie)
		: fStrategy(this)
	{
		if (benaphore_init(&fLock, name) >= B_OK) {
			base_cache_init(this, name, objectSize, alignment, constructor,
				destructor, cookie);
			register_low_memory_handler(_LowMemory, this, 0);
		}
	}

	~Cache()
	{
		if (fLock.sem >= B_OK) {
			benaphore_lock(&fLock);
			unregister_low_memory_handler(_LowMemory, this);
			base_cache_destroy(this, _ReturnSlab);
			benaphore_destroy(&fLock);
		}
	}

	status_t InitCheck() const { return fLock.sem; }

	void *AllocateObject(uint32_t flags)
	{
		BenaphoreLocker _(fLock);

		void *object = base_cache_allocate_object(this);

		// if the cache is returning NULL it is because it ran out of slabs
		if (object == NULL) {
			cache_slab *newSlab = fStrategy.NewSlab(flags);
			if (newSlab == NULL)
				return NULL;
			object = base_cache_allocate_object_with_new_slab(this, newSlab);
			if (object == NULL)
				panic("cache: failed to allocate with an empty slab");
		}

		return object;
	}

	void ReturnObject(void *object)
	{
		BenaphoreLocker _(fLock);

		cache_slab *slab = fStrategy.ObjectSlab(object);

		if (base_cache_return_object(this, slab, object))
			fStrategy.ReturnSlab(slab);
	}

private:
	static void _ReturnSlab(base_cache *self, cache_slab *slab)
	{
		// Already locked, ~Cache() -> base_cache_destroy -> _ReturnSlab
		((ThisCache *)self)->fStrategy.ReturnSlab(slab);
	}

	static void _LowMemory(void *_self, int32 level)
	{
		if (level == B_NO_LOW_MEMORY)
			return;

		ThisCache *self = (ThisCache *)_self;

		BenaphoreLocker _(self->fLock);
		base_cache_low_memory(self, level, _ReturnSlab);
	}

	benaphore fLock;
	Strategy fStrategy;
};

#endif /* __cplusplus */

#endif
