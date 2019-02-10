#pragma once

#include "linalg.h"
#include "Geometry.h"
#include "system-gl.h"
#include "GeometryUtils.h"
#include "renderer.h"
#include "Polygon2d.h"
#include <vector>
#include <string>

#include <boost/logic/tribool.hpp>
BOOST_TRIBOOL_THIRD_STATE(unknown)

class PolySet : public Geometry
{
public:
	PolySet(const PolySet &ps);
	PolySet(unsigned int dim, boost::tribool convex = unknown);
	PolySet(const Polygon2d &origin);
	virtual ~PolySet();

	virtual size_t memsize() const;
	virtual BoundingBox getBoundingBox() const;
	virtual std::string dump() const;
	virtual unsigned int getDimension() const { return this->dim; }
	virtual bool isEmpty() const { return polygons.size() == 0; }
	virtual Geometry *copy() const { return new PolySet(*this); }

	Polygons &getPolygons() { return polygons; }
	const Polygons &getPolygons() const { return polygons; }
	size_t numPolygons() const { return polygons.size(); }

	void append_poly();
	void append_poly(const Polygon &poly);
	void append_vertex(double x, double y, double z = 0.0);
	void append_vertex(const Vector3d &v);
	void append_vertex(const Vector3f &v);
	void insert_vertex(double x, double y, double z = 0.0);
	void insert_vertex(const Vector3d &v);
	void insert_vertex(const Vector3f &v);
	void append(const PolySet &ps);

	void render_surface(Renderer::csgmode_e csgmode, bool mirrored) const;
	void render_edges(Renderer::csgmode_e csgmode) const;
	
	void translate(const Vector3d &translation);
	void transform(const Transform3d &mat);
	void resize(const Vector3d &newsize, const Eigen::Matrix<bool,3,1> &autosize);

	bool is_convex() const;
	boost::tribool convexValue() const { return this->convex; }

	size_t poly_dim() const { return polyDim; }

protected:
	Polygons polygons;
	std::shared_ptr<Polygon2d> polygon;
	unsigned int dim;
	boost::tribool convex;
	BoundingBox bbox;
	size_t polyDim;
	std::string name;

	enum DisplayLists
	{
		None,
		Normal,
		Mirror,
		NormalDiff,
		MirrorDiff,
		Edges,
		EdgesDiff,
		MAX_DISPLAY_LISTS
	};
	mutable unsigned int displayLists[MAX_DISPLAY_LISTS] = { };

	void resetDisplayLists();
};

class QuantizedPolySet : public PolySet
{
	void quantizeVertices();

public:
	explicit QuantizedPolySet(const PolySet &ps) 
		: PolySet(ps), grid(GRID_FINE)
	{
		if (auto qps = dynamic_cast<const QuantizedPolySet*>(&ps))
			grid = qps->grid;
		else
			quantizeVertices();
	}

	const Grid3d<size_t> &getGrid() const { return grid; }

protected:
	Grid3d<size_t> grid;
};

class PolySetValidator
{
public:
	bool validate(const PolySet &ps);
};
