#include "CGALCache.h"
#include "printutils.h"
#include "CGAL_Nef_polyhedron.h"

CGALCache *CGALCache::inst = NULL;

CGALCache::CGALCache(size_t limit) : cache(limit)
{
}

shared_ptr<const Geometry> CGALCache::get(const std::string &id) const
{
	return getNEF(id);
}

shared_ptr<const CGAL_Nef_polyhedron> CGALCache::getNEF(const std::string &id) const
{
	const shared_ptr<const CGAL_Nef_polyhedron> &N = this->cache[id]->N;
#ifdef DEBUG
	PRINTB("CGAL Cache hit: %s (%d bytes)", id.substr(0, 40) % (N ? N->memsize() : 0));
#endif
	return N;
}

bool CGALCache::insert(const std::string &id, const shared_ptr<const Geometry> &N)
{
	return insertNEF(id, dynamic_pointer_cast<const CGAL_Nef_polyhedron>(N));
}

bool CGALCache::insertNEF(const std::string &id, const shared_ptr<const CGAL_Nef_polyhedron> &N)
{
	bool inserted = this->cache.insert(id, new cache_entry(N), N ? N->memsize() : 0);
#ifdef DEBUG
	if (inserted) PRINTB("CGAL Cache insert: %s (%d bytes)", id.substr(0, 40) % (N ? N->memsize() : 0));
	else PRINTB("CGAL Cache insert failed: %s (%d bytes)", id.substr(0, 40) % (N ? N->memsize() : 0));
#endif
	return inserted;
}

bool CGALCache::remove(const std::string &id)
{
	if (cache_entry *entry = this->cache[id]) {
		shared_ptr<const CGAL_Nef_polyhedron> geom = entry->N;
#ifdef DEBUG
		PRINTDB("Geometry Cache remove: %s (%d bytes)", id.substr(0, 40) % (geom ? geom->memsize() : 0));
#endif
		this->cache.remove(id);
		return true;
	}
	return false;
}

size_t CGALCache::maxSize() const
{
	return this->cache.maxCost();
}

void CGALCache::setMaxSize(size_t limit)
{
	this->cache.setMaxCost(limit);
}

void CGALCache::clear()
{
	cache.clear();
}

void CGALCache::print() const
{
	PRINTB("CGAL Polyhedrons in cache: %d", this->cache.size());
	PRINTB("CGAL cache size in bytes: %d", this->cache.totalCost());
}

CGALCache::cache_entry::cache_entry(const shared_ptr<const CGAL_Nef_polyhedron> &N)
	: N(N)
{
	if (print_messages_stack.size() > 0) this->msg = print_messages_stack.back();
}
