#pragma once

#include <stddef.h>
#include <string>
#include <vector>
#include <unordered_map>

#include "linalg.h"
#include "memory.h"

#include "enums.h"

#include "maybe_const.h"

#include "Handles.h"

namespace GeomUtils
{
	// apply OpenSCAD operator
	ResultObject apply(const NodeGeometries &src, OpenSCADOperator op, int dim = 0);
	ResultObject apply(const GeometryHandles &src, OpenSCADOperator op, int dim = 0);

	// collect geometry handles
	void collect(const NodeGeometries &src, GeometryHandles &dest, int &dim, bool unionFirst = false, bool unionGroups = false);
	void collect(const GeometryHandles &src, GeometryHandles &dest, int &dim, bool unionFirst = false, bool unionGroups = false);

	// collect polygon handles
	void collect(const NodeGeometries &src, Polygon2dHandles &dest, bool unionFirst = false, bool unionGroups = false);
	void collect(const GeometryHandles &src, Polygon2dHandles &dest, bool unionFirst = false, bool unionGroups = false);

	// collect geometries
	void collect(const NodeGeometries &src, Geometries &dest, int &dim);
	void collect(const GeometryHandles &src, Geometries &dest, int &dim);

	// collect polygons
	void collect(const NodeGeometries &src, Polygon2ds &dest);
	void collect(const GeometryHandles &src, Polygon2ds &dest);
	void collect(const Geometries &src, Polygon2ds &dest);

	// simplify groups
	GeometryHandle simplify(const GeometryHandle &g);
	ResultObject simplify(const ResultObject &res);

	// determines the diagonal to triangulate a quad
	bool splitfirst(const Vector3d &pp0, const Vector3d &pp1, const Vector3d &pp2, const Vector3d &pp3);
}

class Geometry
{
public:
	struct GeometryData
	{
		const class GeometryGroup *group;
		const class Polygon2d *polygon;
		const class Skelegon2d *skelegon;
		const class PolySet *polySet;
		const class CGAL_Nef_polyhedron *nef;

		GeometryData()
			: group(nullptr)
			, polygon(nullptr)
			, skelegon(nullptr)
			, polySet(nullptr)
			, nef(nullptr)
		{
		}

		void reset(class GeometryGroup *group)
		{
			*this = GeometryData();
			this->group = group;
		}
		void reset(const class Polygon2d *polygon2d)
		{
			*this = GeometryData();
			this->polygon = polygon2d;
		}
		void reset(const class Skelegon2d *skelegon2d)
		{
			*this = GeometryData();
			this->skelegon = skelegon2d;
		}
		void reset(class PolySet *polySet)
		{
			*this = GeometryData();
			this->polySet = polySet;
		}
		void reset(class CGAL_Nef_polyhedron *nef)
		{
			*this = GeometryData();
			this->nef = nef;
		}
	};

	Geometry() : convexity(1) {}
	virtual ~Geometry() throw() {}

	virtual size_t memsize() const = 0;
	virtual BoundingBox getBoundingBox() const = 0;
	virtual std::string dump() const = 0;
	virtual unsigned int getDimension() const = 0;
	virtual bool isEmpty() const = 0;
	virtual Geometry *copy() const = 0;

	unsigned int getConvexity() const { return convexity; }
	void setConvexity(int c) { this->convexity = c; }

	mutable GeometryData data;

protected:
	int convexity;
	std::string type;
};

class EmptyGeometry : public Geometry
{
public:
	virtual size_t memsize() const { return 0; }
	virtual BoundingBox getBoundingBox() const { return BoundingBox(); }
	virtual std::string dump() const { return ""; }
	virtual unsigned int getDimension() const { return 0; }
	virtual bool isEmpty() const { return true; }
	virtual Geometry *copy() const { return new EmptyGeometry(); }
};

class ErrorGeometry : public Geometry
{
public:
	virtual size_t memsize() const { return 0; }
	virtual BoundingBox getBoundingBox() const { return BoundingBox(); }
	virtual std::string dump() const { return ""; }
	virtual unsigned int getDimension() const { return 0; }
	virtual bool isEmpty() const { return true; }
	virtual Geometry *copy() const { return new EmptyGeometry(); }
};

class GeometryGroup : public Geometry
{
public:
	GeometryGroup(const NodeGeometries &kids) : children(kids) {
		type = "GeometryGroup";
		data.group = this;
	}
	virtual size_t memsize() const {
		size_t result = 0;
		return result;
	}
	virtual BoundingBox getBoundingBox() const {
		BoundingBox result;
		for (const auto &child : children)
			if (child.second)
				result.extend(child.second->getBoundingBox());
		return result;
	}
	virtual std::string dump() const {
		std::stringstream result;
		for (const auto &child : children)
			if (child.second)
				result << "\n" << child.second->dump();
		return result.str();
	}
	virtual unsigned int getDimension() const {
		return !isEmpty() ? children.front().second->getDimension() : 0;
	}
	virtual bool isEmpty() const {
		for (const auto &child : children)
			if (child.second)
				if (!child.second->isEmpty())
					return false;
		return true;
	}
	virtual Geometry *copy() const {
		return new GeometryGroup(children);
	}
	const NodeGeometries &getChildren() const {
		return children;
	}
protected:
	NodeGeometries children;
};

class RenderGeometry
{
public:
	RenderGeometry(const GeometryHandle &geom) 
		: geom(geom)
		, transform(Transform3d::Identity())
		, color(-1.0f, -1.0f, -1.0f, -1.0f)
		, highlight(false), background(false)
	{
	}
	GeometryHandle geom;
	Transform3d transform;
	Color4f color;
	bool highlight;
	bool background;
};