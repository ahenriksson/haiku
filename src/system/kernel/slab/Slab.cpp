/*
 * Copyright 2008, Axel Dörfler. All Rights Reserved.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 *
 * Distributed under the terms of the MIT License.
 */


#include <Slab.h>
#include "slab_private.h"

#include <algorithm>
#include <new>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <KernelExport.h>

#include <condition_variable.h>
#include <Depot.h>
#include <kernel.h>
#include <low_resource_manager.h>
#include <smp.h>
#include <tracing.h>
#include <util/AutoLock.h>
#include <util/DoublyLinkedList.h>
#include <util/OpenHashTable.h>
#include <util/khash.h>
#include <vm.h>
#include <vm_address_space.h>


// TODO kMagazineCapacity should be dynamically tuned per cache.


//#define TRACE_SLAB

#ifdef TRACE_SLAB
#define TRACE_CACHE(cache, format, args...) \
	dprintf("Cache[%p, %s] " format "\n", cache, cache->name , ##args)
#else
#define TRACE_CACHE(cache, format, bananas...) do { } while (0)
#endif

#define COMPONENT_PARANOIA_LEVEL	OBJECT_CACHE_PARANOIA
#include <debug_paranoia.h>


#define CACHE_ALIGN_ON_SIZE	(30 << 1)

static const int kMagazineCapacity = 32;
static const size_t kCacheColorPeriod = 8;

struct object_link {
	struct object_link *next;
};

struct slab : DoublyLinkedListLinkImpl<slab> {
	void *pages;
	size_t size;		// total number of objects
	size_t count;		// free objects
	size_t offset;
	object_link *free;
};

typedef DoublyLinkedList<slab> SlabList;
struct ResizeRequest;

struct object_cache : DoublyLinkedListLinkImpl<object_cache> {
	char name[32];
	mutex lock;
	size_t object_size;
	size_t cache_color_cycle;
	SlabList empty, partial, full;
	size_t total_objects;		// total number of objects
	size_t used_count;			// used objects
	size_t empty_count;			// empty slabs
	size_t pressure;
	size_t min_object_reserve;	// minimum number of free objects

	size_t slab_size;
	size_t usage, maximum;
	uint32 flags;

	ResizeRequest *resize_request;

	void *cookie;
	object_cache_constructor constructor;
	object_cache_destructor destructor;
	object_cache_reclaimer reclaimer;

	status_t (*allocate_pages)(object_cache *cache, void **pages,
		uint32 flags, bool unlockWhileAllocating);
	void (*free_pages)(object_cache *cache, void *pages);

	object_depot depot;

	virtual slab *CreateSlab(uint32 flags, bool unlockWhileAllocating) = 0;
	virtual void ReturnSlab(slab *slab) = 0;
	virtual slab *ObjectSlab(void *object) const = 0;

	slab *InitSlab(slab *slab, void *pages, size_t byteCount);
	void UninitSlab(slab *slab);

	virtual status_t PrepareObject(slab *source, void *object) { return B_OK; }
	virtual void UnprepareObject(slab *source, void *object) {}

	virtual ~object_cache() {}

	bool Lock()		{ return mutex_lock(&lock) == B_OK; }
	void Unlock()	{ mutex_unlock(&lock); }
};

typedef DoublyLinkedList<object_cache> ObjectCacheList;

struct SmallObjectCache : object_cache {
	slab *CreateSlab(uint32 flags, bool unlockWhileAllocating);
	void ReturnSlab(slab *slab);
	slab *ObjectSlab(void *object) const;
};


struct HashedObjectCache : object_cache {
	struct Link {
		const void*	buffer;
		slab*		parent;
		Link*		next;
	};

	struct Definition {
		typedef HashedObjectCache	ParentType;
		typedef const void *		KeyType;
		typedef Link				ValueType;

		Definition(HashedObjectCache *_parent) : parent(_parent) {}
		Definition(const Definition& definition) : parent(definition.parent) {}

		size_t HashKey(const void *key) const
		{
			return (((const uint8 *)key) - ((const uint8 *)0))
				>> parent->lower_boundary;
		}

		size_t Hash(Link *value) const { return HashKey(value->buffer); }
		bool Compare(const void *key, Link *value) const
			{ return value->buffer == key; }
		Link*& GetLink(Link *value) const { return value->next; }

		HashedObjectCache *parent;
	};

	typedef BOpenHashTable<Definition> HashTable;

	HashedObjectCache()
		: hash_table(this) {}

	slab *CreateSlab(uint32 flags, bool unlockWhileAllocating);
	void ReturnSlab(slab *slab);
	slab *ObjectSlab(void *object) const;
	status_t PrepareObject(slab *source, void *object);
	void UnprepareObject(slab *source, void *object);

	HashTable hash_table;
	size_t lower_boundary;
};


struct depot_magazine {
	struct depot_magazine *next;
	uint16 current_round, round_count;
	void *rounds[0];
};


struct depot_cpu_store {
	recursive_lock lock;
	struct depot_magazine *loaded, *previous;
};

struct ResizeRequest : DoublyLinkedListLinkImpl<ResizeRequest> {
	ResizeRequest(object_cache* cache)
		:
		cache(cache),
		pending(false),
		delete_when_done(false)
	{
	}

	object_cache*	cache;
	bool			pending;
	bool			delete_when_done;
};

typedef DoublyLinkedList<ResizeRequest> ResizeRequestQueue;


static ObjectCacheList sObjectCaches;
static mutex sObjectCacheListLock = MUTEX_INITIALIZER("object cache list");

static uint8 *sInitialBegin, *sInitialLimit, *sInitialPointer;
static kernel_args *sKernelArgs;


static status_t object_cache_reserve_internal(object_cache *cache,
	size_t object_count, uint32 flags, bool unlockWhileAllocating);
static depot_magazine *alloc_magazine();
static void free_magazine(depot_magazine *magazine);

static mutex sResizeRequestsLock
	= MUTEX_INITIALIZER("object cache resize requests");
static ResizeRequestQueue sResizeRequests;
static ConditionVariable sResizeRequestsCondition;


#if OBJECT_CACHE_TRACING


namespace ObjectCacheTracing {

class ObjectCacheTraceEntry : public AbstractTraceEntry {
	public:
		ObjectCacheTraceEntry(object_cache* cache)
			:
			fCache(cache)
		{
		}

	protected:
		object_cache*	fCache;
};


class Create : public ObjectCacheTraceEntry {
	public:
		Create(const char* name, size_t objectSize, size_t alignment,
				size_t maxByteUsage, uint32 flags, void *cookie,
				object_cache *cache)
			:
			ObjectCacheTraceEntry(cache),
			fObjectSize(objectSize),
			fAlignment(alignment),
			fMaxByteUsage(maxByteUsage),
			fFlags(flags),
			fCookie(cookie)
		{
			fName = alloc_tracing_buffer_strcpy(name, 64, false);
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("object cache create: name: \"%s\", object size: %lu, "
				"alignment: %lu, max usage: %lu, flags: 0x%lx, cookie: %p "
				"-> cache: %p", fName, fObjectSize, fAlignment, fMaxByteUsage,
					fFlags, fCookie, fCache);
		}

	private:
		const char*	fName;
		size_t		fObjectSize;
		size_t		fAlignment;
		size_t		fMaxByteUsage;
		uint32		fFlags;
		void*		fCookie;
};


class Delete : public ObjectCacheTraceEntry {
	public:
		Delete(object_cache *cache)
			:
			ObjectCacheTraceEntry(cache)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("object cache delete: %p", fCache);
		}
};


class Alloc : public ObjectCacheTraceEntry {
	public:
		Alloc(object_cache *cache, uint32 flags, void* object)
			:
			ObjectCacheTraceEntry(cache),
			fFlags(flags),
			fObject(object)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("object cache alloc: cache: %p, flags: 0x%lx -> "
				"object: %p", fCache, fFlags, fObject);
		}

	private:
		uint32		fFlags;
		void*		fObject;
};


class Free : public ObjectCacheTraceEntry {
	public:
		Free(object_cache *cache, void* object)
			:
			ObjectCacheTraceEntry(cache),
			fObject(object)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("object cache free: cache: %p, object: %p", fCache,
				fObject);
		}

	private:
		void*		fObject;
};


class Reserve : public ObjectCacheTraceEntry {
	public:
		Reserve(object_cache *cache, size_t count, uint32 flags)
			:
			ObjectCacheTraceEntry(cache),
			fCount(count),
			fFlags(flags)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("object cache reserve: cache: %p, count: %lu, "
				"flags: 0x%lx", fCache, fCount, fFlags);
		}

	private:
		uint32		fCount;
		uint32		fFlags;
};


}	// namespace ObjectCacheTracing

#	define T(x)	new(std::nothrow) ObjectCacheTracing::x

#else
#	define T(x)
#endif	// OBJECT_CACHE_TRACING


// #pragma mark -


template<typename Type> static Type *
_pop(Type* &head)
{
	Type *oldHead = head;
	head = head->next;
	return oldHead;
}


template<typename Type> static inline void
_push(Type* &head, Type *object)
{
	object->next = head;
	head = object;
}


static inline void *
link_to_object(object_link *link, size_t objectSize)
{
	return ((uint8 *)link) - (objectSize - sizeof(object_link));
}


static inline object_link *
object_to_link(void *object, size_t objectSize)
{
	return (object_link *)(((uint8 *)object)
		+ (objectSize - sizeof(object_link)));
}


static inline depot_cpu_store *
object_depot_cpu(object_depot *depot)
{
	return &depot->stores[smp_get_current_cpu()];
}


static inline int
__fls0(size_t value)
{
	if (value == 0)
		return -1;

	int bit;
	for (bit = 0; value != 1; bit++)
		value >>= 1;
	return bit;
}


static void *
internal_alloc(size_t size, uint32 flags)
{
	if (flags & CACHE_DURING_BOOT) {
		if ((sInitialPointer + size) > sInitialLimit) {
			panic("internal_alloc: ran out of initial space");
			return NULL;
		}

		uint8 *buffer = sInitialPointer;
		sInitialPointer += size;
		return buffer;
	}

	// TODO: Support CACHE_DONT_SLEEP!
	return block_alloc(size);
}


static void
internal_free(void *_buffer)
{
	uint8 *buffer = (uint8 *)_buffer;

	if (buffer >= sInitialBegin && buffer < sInitialLimit)
		return;

	block_free(buffer);
}


static status_t
area_allocate_pages(object_cache *cache, void **pages, uint32 flags,
	bool unlockWhileAllocating)
{
	TRACE_CACHE(cache, "allocate pages (%lu, 0x0%lx)", cache->slab_size, flags);

	uint32 lock = B_FULL_LOCK;
	if (cache->flags & CACHE_UNLOCKED_PAGES)
		lock = B_NO_LOCK;

	uint32 addressSpec = B_ANY_KERNEL_ADDRESS;
	if ((cache->flags & CACHE_ALIGN_ON_SIZE) != 0
		&& cache->slab_size != B_PAGE_SIZE)
		addressSpec = B_ANY_KERNEL_BLOCK_ADDRESS;

	if (unlockWhileAllocating)
		cache->Unlock();

	// if we are allocating, it is because we need the pages immediatly
	// so we lock them. when moving the slab to the empty list we should
	// unlock them, and lock them again when getting one from the empty list.
	area_id areaId = create_area_etc(VMAddressSpace::KernelID(),
		cache->name, pages, addressSpec, cache->slab_size, lock,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA, 0,
		(flags & CACHE_DONT_SLEEP) != 0 ? CREATE_AREA_DONT_WAIT : 0);

	if (unlockWhileAllocating)
		cache->Lock();

	if (areaId < 0)
		return areaId;

	cache->usage += cache->slab_size;

	TRACE_CACHE(cache, "  ... = { %ld, %p }", areaId, *pages);

	return B_OK;
}


static void
area_free_pages(object_cache *cache, void *pages)
{
	area_id id = area_for(pages);

	TRACE_CACHE(cache, "delete pages %p (%ld)", pages, id);

	if (id < 0) {
		panic("object cache: freeing unknown area");
		return;
	}

	delete_area(id);

	cache->usage -= cache->slab_size;
}


static status_t
early_allocate_pages(object_cache *cache, void **pages, uint32 flags,
	bool unlockWhileAllocating)
{
	TRACE_CACHE(cache, "early allocate pages (%lu, 0x0%lx)", cache->slab_size,
		flags);

	if (unlockWhileAllocating)
		cache->Unlock();

	addr_t base = vm_allocate_early(sKernelArgs, cache->slab_size,
		cache->slab_size, B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA);

	if (unlockWhileAllocating)
		cache->Lock();

	*pages = (void *)base;

	cache->usage += cache->slab_size;

	TRACE_CACHE(cache, "  ... = { %p }", *pages);

	return B_OK;
}


static void
early_free_pages(object_cache *cache, void *pages)
{
	panic("memory pressure on bootup?");
}


static void
object_cache_low_memory(void *_self, uint32 resources, int32 level)
{
	if (level == B_NO_LOW_RESOURCE)
		return;

	object_cache *cache = (object_cache *)_self;

	// we are calling the reclaimer without the object cache lock
	// to give the owner a chance to return objects to the slabs.
	// As we only register the low memory handler after we complete
	// the init of the object cache, unless there are some funky
	// memory re-order business going on, we should be ok.

	if (cache->reclaimer)
		cache->reclaimer(cache->cookie, level);

	MutexLocker _(cache->lock);
	size_t minimumAllowed;

	switch (level) {
	case B_LOW_RESOURCE_NOTE:
		minimumAllowed = cache->pressure / 2 + 1;
		break;

	case B_LOW_RESOURCE_WARNING:
		cache->pressure /= 2;
		minimumAllowed = 0;
		break;

	default:
		cache->pressure = 0;
		minimumAllowed = 0;
		break;
	}

	// If the object cache has minimum object reserve, make sure that we don't
	// free too many slabs.
	if (cache->min_object_reserve > 0 && cache->empty_count > 0) {
		size_t objectsPerSlab = cache->empty.Head()->size;
		size_t freeObjects = cache->total_objects - cache->used_count;

		if (cache->min_object_reserve + objectsPerSlab >= freeObjects)
			return;

		size_t slabsToFree = (freeObjects - cache->min_object_reserve)
			/ objectsPerSlab;

		if (cache->empty_count > minimumAllowed + slabsToFree)
			minimumAllowed = cache->empty_count - slabsToFree;
	}

	if (cache->empty_count <= minimumAllowed)
		return;

	TRACE_CACHE(cache, "cache: memory pressure, will release down to %lu.",
		minimumAllowed);

	while (cache->empty_count > minimumAllowed) {
		cache->ReturnSlab(cache->empty.RemoveHead());
		cache->empty_count--;
	}
}


static void
object_cache_return_object_wrapper(object_depot *depot, void *object)
{
	// TODO: the offset calculation might be wrong because we hardcode a
	// SmallObjectCache instead of a base object_cache. Also this must
	// have an unacceptable overhead.
	SmallObjectCache dummyCache;
	object_cache *cache = (object_cache *)(((uint8 *)depot)
		- offset_of_member(dummyCache, depot));

	object_cache_free(cache, object);
}


static status_t
object_cache_init(object_cache *cache, const char *name, size_t objectSize,
	size_t alignment, size_t maximum, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer)
{
	strlcpy(cache->name, name, sizeof(cache->name));

	mutex_init(&cache->lock, cache->name);

	if (objectSize < sizeof(object_link))
		objectSize = sizeof(object_link);

	if (alignment > 0 && (objectSize & (alignment - 1)))
		cache->object_size = objectSize + alignment
			- (objectSize & (alignment - 1));
	else
		cache->object_size = objectSize;

	TRACE_CACHE(cache, "init %lu, %lu -> %lu", objectSize, alignment,
		cache->object_size);

	cache->cache_color_cycle = 0;
	cache->total_objects = 0;
	cache->used_count = 0;
	cache->empty_count = 0;
	cache->pressure = 0;
	cache->min_object_reserve = 0;

	cache->usage = 0;
	cache->maximum = maximum;

	cache->flags = flags;

	cache->resize_request = NULL;

	// TODO: depot destruction is obviously broken
	// no gain in using the depot in single cpu setups
	//if (smp_get_num_cpus() == 1)
		cache->flags |= CACHE_NO_DEPOT;

	if (!(cache->flags & CACHE_NO_DEPOT)) {
		status_t status = object_depot_init(&cache->depot, flags,
			object_cache_return_object_wrapper);
		if (status < B_OK) {
			mutex_destroy(&cache->lock);
			return status;
		}
	}

	cache->cookie = cookie;
	cache->constructor = constructor;
	cache->destructor = destructor;
	cache->reclaimer = reclaimer;

	if (cache->flags & CACHE_DURING_BOOT) {
		cache->allocate_pages = early_allocate_pages;
		cache->free_pages = early_free_pages;
	} else {
		cache->allocate_pages = area_allocate_pages;
		cache->free_pages = area_free_pages;
	}

	register_low_resource_handler(object_cache_low_memory, cache,
		B_KERNEL_RESOURCE_PAGES | B_KERNEL_RESOURCE_MEMORY
			| B_KERNEL_RESOURCE_ADDRESS_SPACE, 5);

	MutexLocker _(sObjectCacheListLock);
	sObjectCaches.Add(cache);

	return B_OK;
}


static void
object_cache_commit_slab(object_cache *cache, slab *slab)
{
	void *pages = (void *)ROUNDDOWN((addr_t)slab->pages, B_PAGE_SIZE);
	if (create_area(cache->name, &pages, B_EXACT_ADDRESS, cache->slab_size,
		B_ALREADY_WIRED, B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA) < 0)
		panic("failed to create_area()");
}


static void
object_cache_commit_pre_pages(object_cache *cache)
{
	SlabList::Iterator it = cache->full.GetIterator();
	while (it.HasNext())
		object_cache_commit_slab(cache, it.Next());

	it = cache->partial.GetIterator();
	while (it.HasNext())
		object_cache_commit_slab(cache, it.Next());

	it = cache->empty.GetIterator();
	while (it.HasNext())
		object_cache_commit_slab(cache, it.Next());

	cache->allocate_pages = area_allocate_pages;
	cache->free_pages = area_free_pages;
}


static status_t
object_cache_resizer(void*)
{
	while (true) {
		MutexLocker locker(sResizeRequestsLock);

		// wait for the next request
		while (sResizeRequests.IsEmpty()) {
			ConditionVariableEntry entry;
			sResizeRequestsCondition.Add(&entry);
			locker.Unlock();
			entry.Wait();
			locker.Lock();
		}

		ResizeRequest* request = sResizeRequests.RemoveHead();

		locker.Unlock();

		// resize the cache, if necessary

		object_cache* cache = request->cache;

		MutexLocker cacheLocker(cache->lock);

		size_t freeObjects = cache->total_objects - cache->used_count;

		while (freeObjects < cache->min_object_reserve) {
			status_t error = object_cache_reserve_internal(cache,
				cache->min_object_reserve - freeObjects, 0, true);
			if (error != B_OK) {
				dprintf("object cache resizer: Failed to resize object cache "
					"%p!\n", cache);
				break;
			}

			freeObjects = cache->total_objects - cache->used_count;
		}

		request->pending = false;

		if (request->delete_when_done)
			delete request;
	}

	// never can get here
	return B_OK;
}


static void
increase_object_reserve(object_cache* cache)
{
	if (cache->resize_request->pending)
		return;

	cache->resize_request->pending = true;

	MutexLocker locker(sResizeRequestsLock);
	sResizeRequests.Add(cache->resize_request);
	sResizeRequestsCondition.NotifyAll();
}


static void
delete_cache(object_cache *cache)
{
	cache->~object_cache();
	internal_free(cache);
}


static SmallObjectCache *
create_small_object_cache(const char *name, size_t object_size,
	size_t alignment, size_t maximum, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer)
{
	void *buffer = internal_alloc(sizeof(SmallObjectCache), flags);
	if (buffer == NULL)
		return NULL;

	SmallObjectCache *cache = new (buffer) SmallObjectCache();

	if (object_cache_init(cache, name, object_size, alignment, maximum,
			flags | CACHE_ALIGN_ON_SIZE, cookie, constructor, destructor,
			reclaimer) < B_OK) {
		delete_cache(cache);
		return NULL;
	}

	if ((flags & CACHE_LARGE_SLAB) != 0)
		cache->slab_size = max_c(16 * B_PAGE_SIZE, 1024 * object_size);
	else
		cache->slab_size = B_PAGE_SIZE;

	return cache;
}


static HashedObjectCache *
create_hashed_object_cache(const char *name, size_t object_size,
	size_t alignment, size_t maximum, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer)
{
	void *buffer = internal_alloc(sizeof(HashedObjectCache), flags);
	if (buffer == NULL)
		return NULL;

	HashedObjectCache *cache = new (buffer) HashedObjectCache();

	if (object_cache_init(cache, name, object_size, alignment, maximum, flags,
		cookie, constructor, destructor, reclaimer) < B_OK) {
		delete_cache(cache);
		return NULL;
	}

	if ((flags & CACHE_LARGE_SLAB) != 0)
		cache->slab_size = max_c(256 * B_PAGE_SIZE, 128 * object_size);
	else
		cache->slab_size = max_c(16 * B_PAGE_SIZE, 8 * object_size);
	cache->lower_boundary = __fls0(cache->object_size);

	return cache;
}


object_cache *
create_object_cache(const char *name, size_t object_size, size_t alignment,
	void *cookie, object_cache_constructor constructor,
	object_cache_destructor destructor)
{
	return create_object_cache_etc(name, object_size, alignment, 0, 0, cookie,
		constructor, destructor, NULL);
}


object_cache *
create_object_cache_etc(const char *name, size_t objectSize, size_t alignment,
	size_t maximum, uint32 flags, void *cookie,
	object_cache_constructor constructor, object_cache_destructor destructor,
	object_cache_reclaimer reclaimer)
{
	object_cache *cache;

	if (objectSize == 0) {
		cache = NULL;
	} else if (objectSize <= 256) {
		cache = create_small_object_cache(name, objectSize, alignment, maximum,
			flags, cookie, constructor, destructor, reclaimer);
	} else {
		cache = create_hashed_object_cache(name, objectSize, alignment,
			maximum, flags, cookie, constructor, destructor, reclaimer);
	}

	T(Create(name, objectSize, alignment, maximum, flags, cookie, cache));
	return cache;
}


void
delete_object_cache(object_cache *cache)
{
	T(Delete(cache));

	{
		MutexLocker _(sObjectCacheListLock);
		sObjectCaches.Remove(cache);
	}

	mutex_lock(&cache->lock);

	if (!(cache->flags & CACHE_NO_DEPOT))
		object_depot_destroy(&cache->depot);

	unregister_low_resource_handler(object_cache_low_memory, cache);

	if (!cache->full.IsEmpty())
		panic("cache destroy: still has full slabs");

	if (!cache->partial.IsEmpty())
		panic("cache destroy: still has partial slabs");

	while (!cache->empty.IsEmpty())
		cache->ReturnSlab(cache->empty.RemoveHead());

	mutex_destroy(&cache->lock);
	delete_cache(cache);
}


status_t
object_cache_set_minimum_reserve(object_cache *cache, size_t objectCount)
{
	MutexLocker _(cache->lock);

	if (cache->min_object_reserve == objectCount)
		return B_OK;

	if (cache->min_object_reserve == 0) {
		cache->resize_request = new(std::nothrow) ResizeRequest(cache);
		if (cache->resize_request == NULL)
			return B_NO_MEMORY;
	} else if (cache->min_object_reserve == 0) {
		if (cache->resize_request->pending)
			cache->resize_request->delete_when_done = true;
		else
			delete cache->resize_request;

		cache->resize_request = NULL;
	}

	cache->min_object_reserve = objectCount;

	increase_object_reserve(cache);

	return B_OK;
}


void *
object_cache_alloc(object_cache *cache, uint32 flags)
{
	if (!(cache->flags & CACHE_NO_DEPOT)) {
		void *object = object_depot_obtain(&cache->depot);
		if (object) {
			T(Alloc(cache, flags, object));
			return object;
		}
	}

	MutexLocker _(cache->lock);
	slab *source;

	if (cache->partial.IsEmpty()) {
		if (cache->empty.IsEmpty()) {
			if (object_cache_reserve_internal(cache, 1, flags, false) < B_OK) {
				T(Alloc(cache, flags, NULL));
				return NULL;
			}

			cache->pressure++;
		}

		source = cache->empty.RemoveHead();
		cache->empty_count--;
		cache->partial.Add(source);
	} else {
		source = cache->partial.Head();
	}

	ParanoiaChecker _2(source);

	object_link *link = _pop(source->free);
	source->count--;
	cache->used_count++;

	if (cache->total_objects - cache->used_count < cache->min_object_reserve)
		increase_object_reserve(cache);

	REMOVE_PARANOIA_CHECK(PARANOIA_SUSPICIOUS, source, &link->next,
		sizeof(void*));

	TRACE_CACHE(cache, "allocate %p (%p) from %p, %lu remaining.",
		link_to_object(link, cache->object_size), link, source, source->count);

	if (source->count == 0) {
		cache->partial.Remove(source);
		cache->full.Add(source);
	}

	void *object = link_to_object(link, cache->object_size);
	T(Alloc(cache, flags, object));
	return object;
}


static void
object_cache_return_to_slab(object_cache *cache, slab *source, void *object)
{
	if (source == NULL) {
		panic("object_cache: free'd object has no slab");
		return;
	}

	ParanoiaChecker _(source);

	object_link *link = object_to_link(object, cache->object_size);

	TRACE_CACHE(cache, "returning %p (%p) to %p, %lu used (%lu empty slabs).",
		object, link, source, source->size - source->count,
		cache->empty_count);

	_push(source->free, link);
	source->count++;
	cache->used_count--;

	ADD_PARANOIA_CHECK(PARANOIA_SUSPICIOUS, source, &link->next, sizeof(void*));

	if (source->count == source->size) {
		cache->partial.Remove(source);

		if (cache->empty_count < cache->pressure
			&& cache->total_objects - cache->used_count - source->size
				>= cache->min_object_reserve) {
			cache->empty_count++;
			cache->empty.Add(source);
		} else {
			cache->ReturnSlab(source);
		}
	} else if (source->count == 1) {
		cache->full.Remove(source);
		cache->partial.Add(source);
	}
}


void
object_cache_free(object_cache *cache, void *object)
{
	if (object == NULL)
		return;

	T(Free(cache, object));

	if (!(cache->flags & CACHE_NO_DEPOT)) {
		if (object_depot_store(&cache->depot, object))
			return;
	}

	MutexLocker _(cache->lock);
	object_cache_return_to_slab(cache, cache->ObjectSlab(object), object);
}


static status_t
object_cache_reserve_internal(object_cache *cache, size_t objectCount,
	uint32 flags, bool unlockWhileAllocating)
{
	size_t numBytes = objectCount * cache->object_size;
	size_t slabCount = ((numBytes - 1) / cache->slab_size) + 1;
		// TODO: This also counts the unusable space of each slab, which can
		// sum up.

	while (slabCount > 0) {
		slab *newSlab = cache->CreateSlab(flags, unlockWhileAllocating);
		if (newSlab == NULL)
			return B_NO_MEMORY;

		cache->empty.Add(newSlab);
		cache->empty_count++;
		slabCount--;
	}

	return B_OK;
}


status_t
object_cache_reserve(object_cache *cache, size_t objectCount, uint32 flags)
{
	if (objectCount == 0)
		return B_OK;

	T(Reserve(cache, objectCount, flags));

	MutexLocker _(cache->lock);
	return object_cache_reserve_internal(cache, objectCount, flags, false);
}


void
object_cache_get_usage(object_cache *cache, size_t *_allocatedMemory)
{
	MutexLocker _(cache->lock);
	*_allocatedMemory = cache->usage;
}


slab *
object_cache::InitSlab(slab *slab, void *pages, size_t byteCount)
{
	TRACE_CACHE(this, "construct (%p, %p .. %p, %lu)", slab, pages,
		((uint8 *)pages) + byteCount, byteCount);

	slab->pages = pages;
	slab->count = slab->size = byteCount / object_size;
	slab->free = NULL;
	total_objects += slab->size;

	size_t spareBytes = byteCount - (slab->size * object_size);
	slab->offset = cache_color_cycle;

	if (slab->offset > spareBytes)
		cache_color_cycle = slab->offset = 0;
	else
		cache_color_cycle += kCacheColorPeriod;

	TRACE_CACHE(this, "  %lu objects, %lu spare bytes, offset %lu",
		slab->size, spareBytes, slab->offset);

	uint8 *data = ((uint8 *)pages) + slab->offset;

	CREATE_PARANOIA_CHECK_SET(slab, "slab");

	for (size_t i = 0; i < slab->size; i++) {
		bool failedOnFirst = false;

		status_t status = PrepareObject(slab, data);
		if (status < B_OK)
			failedOnFirst = true;
		else if (constructor)
			status = constructor(cookie, data);

		if (status < B_OK) {
			if (!failedOnFirst)
				UnprepareObject(slab, data);

			data = ((uint8 *)pages) + slab->offset;
			for (size_t j = 0; j < i; j++) {
				if (destructor)
					destructor(cookie, data);
				UnprepareObject(slab, data);
				data += object_size;
			}

			DELETE_PARANOIA_CHECK_SET(slab);

			return NULL;
		}

		_push(slab->free, object_to_link(data, object_size));

		ADD_PARANOIA_CHECK(PARANOIA_SUSPICIOUS, slab,
			&object_to_link(data, object_size)->next, sizeof(void*));

		data += object_size;
	}

	return slab;
}


void
object_cache::UninitSlab(slab *slab)
{
	TRACE_CACHE(this, "destruct %p", slab);

	if (slab->count != slab->size)
		panic("cache: destroying a slab which isn't empty.");

	total_objects -= slab->size;

	DELETE_PARANOIA_CHECK_SET(slab);

	uint8 *data = ((uint8 *)slab->pages) + slab->offset;

	for (size_t i = 0; i < slab->size; i++) {
		if (destructor)
			destructor(cookie, data);
		UnprepareObject(slab, data);
		data += object_size;
	}
}


static inline slab *
slab_in_pages(const void *pages, size_t slab_size)
{
	return (slab *)(((uint8 *)pages) + slab_size - sizeof(slab));
}


static inline const void *
lower_boundary(void *object, size_t byteCount)
{
	const uint8 *null = (uint8 *)NULL;
	return null + ((((uint8 *)object) - null) & ~(byteCount - 1));
}


static inline bool
check_cache_quota(object_cache *cache)
{
	if (cache->maximum == 0)
		return true;

	return (cache->usage + cache->slab_size) <= cache->maximum;
}


slab *
SmallObjectCache::CreateSlab(uint32 flags, bool unlockWhileAllocating)
{
	if (!check_cache_quota(this))
		return NULL;

	void *pages;

	if (allocate_pages(this, &pages, flags, unlockWhileAllocating) < B_OK)
		return NULL;

	return InitSlab(slab_in_pages(pages, slab_size), pages,
		slab_size - sizeof(slab));
}


void
SmallObjectCache::ReturnSlab(slab *slab)
{
	UninitSlab(slab);
	free_pages(this, slab->pages);
}


slab *
SmallObjectCache::ObjectSlab(void *object) const
{
	return slab_in_pages(lower_boundary(object, slab_size), slab_size);
}


static slab *
allocate_slab(uint32 flags)
{
	return (slab *)internal_alloc(sizeof(slab), flags);
}


static void
free_slab(slab *slab)
{
	internal_free(slab);
}


static HashedObjectCache::Link *
allocate_link(uint32 flags)
{
	return (HashedObjectCache::Link *)
		internal_alloc(sizeof(HashedObjectCache::Link), flags);
}


static void
free_link(HashedObjectCache::Link *link)
{
	internal_free(link);
}


slab *
HashedObjectCache::CreateSlab(uint32 flags, bool unlockWhileAllocating)
{
	if (!check_cache_quota(this))
		return NULL;

	if (unlockWhileAllocating)
		Unlock();

	slab *slab = allocate_slab(flags);

	if (unlockWhileAllocating)
		Lock();

	if (slab == NULL)
		return NULL;

	void *pages;

	if (allocate_pages(this, &pages, flags, unlockWhileAllocating) == B_OK) {
		if (InitSlab(slab, pages, slab_size))
			return slab;

		free_pages(this, pages);
	}

	free_slab(slab);
	return NULL;
}


void
HashedObjectCache::ReturnSlab(slab *slab)
{
	UninitSlab(slab);
	free_pages(this, slab->pages);
}


slab *
HashedObjectCache::ObjectSlab(void *object) const
{
	Link *link = hash_table.Lookup(object);
	if (link == NULL) {
		panic("object cache: requested object %p missing from hash table",
			object);
		return NULL;
	}
	return link->parent;
}


status_t
HashedObjectCache::PrepareObject(slab *source, void *object)
{
	Link *link = allocate_link(CACHE_DONT_SLEEP);
	if (link == NULL)
		return B_NO_MEMORY;

	link->buffer = object;
	link->parent = source;

	hash_table.Insert(link);
	return B_OK;
}


void
HashedObjectCache::UnprepareObject(slab *source, void *object)
{
	Link *link = hash_table.Lookup(object);
	if (link == NULL) {
		panic("object cache: requested object missing from hash table");
		return;
	}

	if (link->parent != source) {
		panic("object cache: slab mismatch");
		return;
	}

	hash_table.Remove(link);
	free_link(link);
}


static inline bool
is_magazine_empty(depot_magazine *magazine)
{
	return magazine->current_round == 0;
}


static inline bool
is_magazine_full(depot_magazine *magazine)
{
	return magazine->current_round == magazine->round_count;
}


static inline void *
pop_magazine(depot_magazine *magazine)
{
	return magazine->rounds[--magazine->current_round];
}


static inline bool
push_magazine(depot_magazine *magazine, void *object)
{
	if (is_magazine_full(magazine))
		return false;
	magazine->rounds[magazine->current_round++] = object;
	return true;
}


static bool
exchange_with_full(object_depot *depot, depot_magazine* &magazine)
{
	RecursiveLocker _(depot->lock);

	if (depot->full == NULL)
		return false;

	depot->full_count--;
	depot->empty_count++;

	_push(depot->empty, magazine);
	magazine = _pop(depot->full);
	return true;
}


static bool
exchange_with_empty(object_depot *depot, depot_magazine* &magazine)
{
	RecursiveLocker _(depot->lock);

	if (depot->empty == NULL) {
		depot->empty = alloc_magazine();
		if (depot->empty == NULL)
			return false;
	} else {
		depot->empty_count--;
	}

	if (magazine) {
		_push(depot->full, magazine);
		depot->full_count++;
	}

	magazine = _pop(depot->empty);
	return true;
}


static depot_magazine *
alloc_magazine()
{
	depot_magazine *magazine = (depot_magazine *)internal_alloc(
		sizeof(depot_magazine) + kMagazineCapacity * sizeof(void *), 0);
	if (magazine) {
		magazine->next = NULL;
		magazine->current_round = 0;
		magazine->round_count = kMagazineCapacity;
	}

	return magazine;
}


static void
free_magazine(depot_magazine *magazine)
{
	internal_free(magazine);
}


static void
empty_magazine(object_depot *depot, depot_magazine *magazine)
{
	for (uint16 i = 0; i < magazine->current_round; i++)
		depot->return_object(depot, magazine->rounds[i]);
	free_magazine(magazine);
}


status_t
object_depot_init(object_depot *depot, uint32 flags,
	void (*return_object)(object_depot *depot, void *object))
{
	depot->full = NULL;
	depot->empty = NULL;
	depot->full_count = depot->empty_count = 0;

	recursive_lock_init(&depot->lock, "depot");

	depot->stores = (depot_cpu_store *)internal_alloc(sizeof(depot_cpu_store)
		* smp_get_num_cpus(), flags);
	if (depot->stores == NULL) {
		recursive_lock_destroy(&depot->lock);
		return B_NO_MEMORY;
	}

	for (int i = 0; i < smp_get_num_cpus(); i++) {
		recursive_lock_init(&depot->stores[i].lock, "cpu store");
		depot->stores[i].loaded = depot->stores[i].previous = NULL;
	}

	depot->return_object = return_object;

	return B_OK;
}


void
object_depot_destroy(object_depot *depot)
{
	object_depot_make_empty(depot);

	for (int i = 0; i < smp_get_num_cpus(); i++) {
		recursive_lock_destroy(&depot->stores[i].lock);
	}

	internal_free(depot->stores);

	recursive_lock_destroy(&depot->lock);
}


static void *
object_depot_obtain_from_store(object_depot *depot, depot_cpu_store *store)
{
	RecursiveLocker _(store->lock);

	// To better understand both the Alloc() and Free() logic refer to
	// Bonwick's ``Magazines and Vmem'' [in 2001 USENIX proceedings]

	// In a nutshell, we try to get an object from the loaded magazine
	// if it's not empty, or from the previous magazine if it's full
	// and finally from the Slab if the magazine depot has no full magazines.

	if (store->loaded == NULL)
		return NULL;

	while (true) {
		if (!is_magazine_empty(store->loaded))
			return pop_magazine(store->loaded);

		if (store->previous && (is_magazine_full(store->previous)
			|| exchange_with_full(depot, store->previous)))
			std::swap(store->previous, store->loaded);
		else
			return NULL;
	}
}


static int
object_depot_return_to_store(object_depot *depot, depot_cpu_store *store,
	void *object)
{
	RecursiveLocker _(store->lock);

	// We try to add the object to the loaded magazine if we have one
	// and it's not full, or to the previous one if it is empty. If
	// the magazine depot doesn't provide us with a new empty magazine
	// we return the object directly to the slab.

	while (true) {
		if (store->loaded && push_magazine(store->loaded, object))
			return 1;

		if ((store->previous && is_magazine_empty(store->previous))
			|| exchange_with_empty(depot, store->previous))
			std::swap(store->loaded, store->previous);
		else
			return 0;
	}
}


void *
object_depot_obtain(object_depot *depot)
{
	return object_depot_obtain_from_store(depot, object_depot_cpu(depot));
}


int
object_depot_store(object_depot *depot, void *object)
{
	return object_depot_return_to_store(depot, object_depot_cpu(depot),
		object);
}


void
object_depot_make_empty(object_depot *depot)
{
	for (int i = 0; i < smp_get_num_cpus(); i++) {
		depot_cpu_store *store = &depot->stores[i];

		RecursiveLocker _(store->lock);

		if (depot->stores[i].loaded)
			empty_magazine(depot, depot->stores[i].loaded);
		if (depot->stores[i].previous)
			empty_magazine(depot, depot->stores[i].previous);
		depot->stores[i].loaded = depot->stores[i].previous = NULL;
	}

	RecursiveLocker _(depot->lock);

	while (depot->full)
		empty_magazine(depot, _pop(depot->full));

	while (depot->empty)
		empty_magazine(depot, _pop(depot->empty));
}


static int
dump_slabs(int argc, char *argv[])
{
	kprintf("%10s %22s %8s %8s %6s %8s %8s %8s\n", "address", "name",
		"objsize", "usage", "empty", "usedobj", "total", "flags");

	ObjectCacheList::Iterator it = sObjectCaches.GetIterator();

	while (it.HasNext()) {
		object_cache *cache = it.Next();

		kprintf("%p %22s %8lu %8lu %6lu %8lu %8lu %8lx\n", cache, cache->name,
			cache->object_size, cache->usage, cache->empty_count,
			cache->used_count, cache->usage / cache->object_size,
			cache->flags);
	}

	return 0;
}


static int
dump_cache_info(int argc, char *argv[])
{
	if (argc < 2) {
		kprintf("usage: cache_info [address]\n");
		return 0;
	}

	object_cache *cache = (object_cache *)strtoul(argv[1], NULL, 16);

	kprintf("name: %s\n", cache->name);
	kprintf("lock: %p\n", &cache->lock);
	kprintf("object_size: %lu\n", cache->object_size);
	kprintf("cache_color_cycle: %lu\n", cache->cache_color_cycle);
	kprintf("used_count: %lu\n", cache->used_count);
	kprintf("empty_count: %lu\n", cache->empty_count);
	kprintf("pressure: %lu\n", cache->pressure);
	kprintf("slab_size: %lu\n", cache->slab_size);
	kprintf("usage: %lu\n", cache->usage);
	kprintf("maximum: %lu\n", cache->maximum);
	kprintf("flags: 0x%lx\n", cache->flags);
	kprintf("cookie: %p\n", cache->cookie);

	return 0;
}


void
slab_init(kernel_args *args, addr_t initialBase, size_t initialSize)
{
	dprintf("slab: init base %p + 0x%lx\n", (void *)initialBase, initialSize);

	sInitialBegin = (uint8 *)initialBase;
	sInitialLimit = sInitialBegin + initialSize;
	sInitialPointer = sInitialBegin;

	sKernelArgs = args;

	new (&sObjectCaches) ObjectCacheList();

	block_allocator_init_boot();

	add_debugger_command("slabs", dump_slabs, "list all object caches");
	add_debugger_command("cache_info", dump_cache_info,
		"dump information about a specific cache");
}


void
slab_init_post_sem()
{
	ObjectCacheList::Iterator it = sObjectCaches.GetIterator();

	while (it.HasNext()) {
		object_cache *cache = it.Next();
		if (cache->allocate_pages == early_allocate_pages)
			object_cache_commit_pre_pages(cache);
	}

	block_allocator_init_rest();
}


void
slab_init_post_thread()
{
	new(&sResizeRequests) ResizeRequestQueue;
	sResizeRequestsCondition.Init(&sResizeRequests, "object cache resizer");

	thread_id objectCacheResizer = spawn_kernel_thread(object_cache_resizer,
		"object cache resizer", B_URGENT_PRIORITY, NULL);
	if (objectCacheResizer < 0) {
		panic("slab_init_post_thread(): failed to spawn object cache resizer "
			"thread\n");
		return;
	}

	send_signal_etc(objectCacheResizer, SIGCONT, B_DO_NOT_RESCHEDULE);
}
