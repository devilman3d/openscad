#pragma once

#include "cache.h"
#include "memory.h"
#include "GeometryCache.h"

/*!
*/
class CGALCache : public IGeometryCache
{
public:	
	CGALCache(size_t limit = 100*1024*1024);

	static CGALCache *instance() { if (!inst) inst = new CGALCache; return inst; }

	virtual bool contains(const std::string &id) const { return this->cache.contains(id); }
	virtual shared_ptr<const class Geometry> get(const std::string &id) const;
	shared_ptr<const class CGAL_Nef_polyhedron> getNEF(const std::string &id) const;
	virtual bool insert(const std::string &id, const shared_ptr<const Geometry> &N);
	virtual bool insertNEF(const std::string &id, const shared_ptr<const CGAL_Nef_polyhedron> &N);
	virtual bool remove(const std::string &id);
	virtual size_t maxSize() const;
	virtual void setMaxSize(size_t limit);
	virtual void clear();
	virtual void print() const;

private:
	static CGALCache *inst;

	struct cache_entry {
		shared_ptr<const CGAL_Nef_polyhedron> N;
		std::string msg;
		cache_entry(const shared_ptr<const CGAL_Nef_polyhedron> &N);
		~cache_entry() { }
	};

	Cache<std::string, cache_entry> cache;
};
