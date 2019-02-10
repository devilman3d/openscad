#pragma once

#include "cache.h"
#include "memory.h"
#include "Geometry.h"

class IGeometryCache
{
public:
	virtual bool contains(const std::string &id) const = 0;
	virtual shared_ptr<const class Geometry> get(const std::string &id) const = 0;
	virtual size_t maxSize() const = 0;
	virtual bool insert(const std::string &id, const shared_ptr<const Geometry> &geom) = 0;
	virtual bool remove(const std::string &id) = 0;
	virtual void setMaxSize(size_t limit) = 0;
	virtual void clear() = 0;
	virtual void print() const = 0;
};

class GeometryCache : public IGeometryCache
{
public:	
	GeometryCache(size_t memorylimit = 100*1024*1024) : cache(memorylimit) {}

	static GeometryCache *instance() { if (!inst) inst = new GeometryCache; return inst; }

	virtual bool contains(const std::string &id) const { return this->cache.contains(id); }
	virtual shared_ptr<const class Geometry> get(const std::string &id) const;
	virtual size_t maxSize() const;
	virtual bool insert(const std::string &id, const shared_ptr<const Geometry> &geom);
	virtual bool remove(const std::string &id);
	virtual void setMaxSize(size_t limit);
	virtual void clear() { cache.clear(); }
	virtual void print() const;

private:
	static GeometryCache *inst;

	struct cache_entry {
		shared_ptr<const class Geometry> geom;
		std::string msg;
		cache_entry(const shared_ptr<const Geometry> &geom);
		~cache_entry() { }
	};

	Cache<std::string, cache_entry> cache;
};
