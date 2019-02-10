/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "module.h"
#include "node.h"
#include "polyset.h"
#include "evalcontext.h"
#include "Polygon2d.h"
#include "builtin.h"
#include "printutils.h"
#include "context.h"
#include "calc.h"
#include "FactoryNode.h"
#include <sstream>
#include <assert.h>
#include <cmath>
#include <boost/assign/std/vector.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

#define F_MINIMUM 0.01

enum primitive_type_e {
	CUBE,
	SPHERE,
	CYLINDER,
	POLYHEDRON,
	SQUARE,
	CIRCLE,
	POLYGON,
	LINE
};

class PrimitiveNode : public FactoryNode
{
public:
	VISITABLE();

	template <typename ... Args>
	PrimitiveNode(primitive_type_e type, Args ... args)
		: FactoryNode(args...)
		, type(type)
		, center(false), open(false)
		, x(0), y(0), z(0), h(0), r1(0), r2(0)
		, fn(0), fs(0), fa(0)
		, points(Value::undefined), paths(Value::undefined), faces(Value::undefined)
	{
	}

	primitive_type_e type;
	bool center;
	bool open; // polyhedrons and polygons can be open or closed (default)
	double x, y, z, h, r1, r2;
	double fn, fs, fa;
	ValuePtr points, paths, faces;

	/**
	 * Return a radius value by looking up both a diameter and radius variable.
	 * The diameter has higher priority, so if found an additionally set radius
	 * value is ignored.
	 *
	 * @param ctx data context with variable values.
	 * @param radius_var name of the variable to lookup for the radius value.
	 * @param diameter_var name of the variable to lookup for the diameter value.
	 * @return radius value of type Value::NUMBER or Value::UNDEFINED if both
	 *         variables are invalid or not set.
	 */
	Value lookup_radius(const Context &ctx, const std::string &diameter_var, const std::string &radius_var) const
	{
		ValuePtr d = ctx.lookup_variable(diameter_var, true);
		ValuePtr r = ctx.lookup_variable(radius_var, true);
		const bool r_defined = (r->type() == Value::NUMBER);

		if (d->type() == Value::NUMBER) {
			if (r_defined) {
				PRINTB("WARNING: Ignoring radius variable '%s' as diameter '%s' is defined too.", radius_var % diameter_var);
			}
			return Value(d->toDouble() / 2.0);
		}
		else if (r_defined) {
			return *r;
		}
		else {
			return Value::undefined;
		}
	}

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		this->center = false;
		this->x = this->y = this->z = this->h = this->r1 = this->r2 = 1;

		this->fn = c.lookup_variable("$fn")->toDouble();
		this->fs = c.lookup_variable("$fs")->toDouble();
		this->fa = c.lookup_variable("$fa")->toDouble();

		if (this->fs < F_MINIMUM) {
			PRINTB("WARNING: $fs too small - clamping to %f", F_MINIMUM);
			this->fs = F_MINIMUM;
		}
		if (this->fa < F_MINIMUM) {
			PRINTB("WARNING: $fa too small - clamping to %f", F_MINIMUM);
			this->fa = F_MINIMUM;
		}

		switch (this->type) {
		case CUBE: {
			ValuePtr size = c.lookup_variable("size");
			ValuePtr center = c.lookup_variable("center");
			size->getDouble(this->x);
			size->getDouble(this->y);
			size->getDouble(this->z);
			size->getVec3(this->x, this->y, this->z);
			if (center->type() == Value::BOOL) {
				this->center = center->toBool();
			}
			break;
		}
		case SPHERE: {
			const Value r = lookup_radius(c, "d", "r");
			if (r.type() == Value::NUMBER) {
				this->r1 = r.toDouble();
			}
			break;
		}
		case CYLINDER: {
			ValuePtr h = c.lookup_variable("h");
			if (h->type() == Value::NUMBER) {
				this->h = h->toDouble();
			}

			const Value r = lookup_radius(c, "d", "r");
			const Value r1 = lookup_radius(c, "d1", "r1");
			const Value r2 = lookup_radius(c, "d2", "r2");
			if (r.type() == Value::NUMBER) {
				this->r1 = r.toDouble();
				this->r2 = r.toDouble();
			}
			if (r1.type() == Value::NUMBER) {
				this->r1 = r1.toDouble();
			}
			if (r2.type() == Value::NUMBER) {
				this->r2 = r2.toDouble();
			}

			ValuePtr center = c.lookup_variable("center");
			if (center->type() == Value::BOOL) {
				this->center = center->toBool();
			}
			break;
		}
		case POLYHEDRON: {
			this->points = c.lookup_variable("points");
			this->faces = c.lookup_variable("faces");
			if (this->faces->type() == Value::UNDEFINED) {
				// backwards compatible
				this->faces = c.lookup_variable("triangles", true);
				if (this->faces->type() != Value::UNDEFINED) {
					printDeprecation("polyhedron(triangles=[]) will be removed in future releases. Use polyhedron(faces=[]) instead.");
				}
			}
			ValuePtr open = c.lookup_variable("open");
			if (open->type() == Value::BOOL) {
				this->open = open->toBool();
			}
			break;
		}
		case SQUARE: {
			ValuePtr size = c.lookup_variable("size");
			ValuePtr center = c.lookup_variable("center");
			size->getDouble(this->x);
			size->getDouble(this->y);
			size->getVec2(this->x, this->y);
			if (center->type() == Value::BOOL) {
				this->center = center->toBool();
			}
			break;
		}
		case CIRCLE: {
			const Value r = lookup_radius(c, "d", "r");
			if (r.type() == Value::NUMBER) {
				this->r1 = r.toDouble();
			}
			break;
		}
		case POLYGON: {
			this->points = c.lookup_variable("points");
			this->paths = c.lookup_variable("paths");
			ValuePtr open = c.lookup_variable("open");
			if (open->type() == Value::BOOL) {
				this->open = open->toBool();
			}
			break;
		}
		case LINE: {
			this->points = c.lookup_variable("points");
			break;
		}
		}
	}

	struct point2d {
		double x, y;
	};

	static void generate_circle(point2d *circle, double r, int fragments)
	{
		for (int i = 0; i < fragments; i++) {
			double phi = (M_PI * 2 * i) / fragments;
			circle[i].x = r * cos(phi);
			circle[i].y = r * sin(phi);
		}
	}

	static void generateCube(const Vector3d &min, const Vector3d &max, PolySet &p)
	{
		generateCube(min[0], min[1], min[2], max[0], max[1], max[2], p);
	}

	static void generateCube(double x1, double y1, double z1, double x2, double y2, double z2, PolySet &p)
	{
		p.append_poly(); // top
		p.append_vertex(x1, y1, z2);
		p.append_vertex(x2, y1, z2);
		p.append_vertex(x2, y2, z2);
		p.append_vertex(x1, y2, z2);

		p.append_poly(); // bottom
		p.append_vertex(x1, y2, z1);
		p.append_vertex(x2, y2, z1);
		p.append_vertex(x2, y1, z1);
		p.append_vertex(x1, y1, z1);

		p.append_poly(); // side1
		p.append_vertex(x1, y1, z1);
		p.append_vertex(x2, y1, z1);
		p.append_vertex(x2, y1, z2);
		p.append_vertex(x1, y1, z2);

		p.append_poly(); // side2
		p.append_vertex(x2, y1, z1);
		p.append_vertex(x2, y2, z1);
		p.append_vertex(x2, y2, z2);
		p.append_vertex(x2, y1, z2);

		p.append_poly(); // side3
		p.append_vertex(x2, y2, z1);
		p.append_vertex(x1, y2, z1);
		p.append_vertex(x1, y2, z2);
		p.append_vertex(x2, y2, z2);

		p.append_poly(); // side4
		p.append_vertex(x1, y2, z1);
		p.append_vertex(x1, y1, z1);
		p.append_vertex(x1, y1, z2);
		p.append_vertex(x1, y2, z2);
	}

	static void generateSphere(double r, double fn, double fs, double fa, PolySet *p)
	{
		struct ring_s {
			std::vector<point2d> points;
			double z;
		};

		int fragments = Calc::get_fragments_from_r(r, fn, fs, fa);
		int rings = (fragments + 1) / 2;
		// Uncomment the following three lines to enable experimental sphere tesselation
		//		if (rings % 2 == 0) rings++; // To ensure that the middle ring is at phi == 0 degrees

		std::vector<ring_s> ring;
		ring.resize(rings);

		//		double offset = 0.5 * ((fragments / 2) % 2);
		for (int i = 0; i < rings; i++) {
			//			double phi = (M_PI * (i + offset)) / (fragments/2);
			double phi = (M_PI * (i + 0.5)) / rings;
			double rr = r * sin(phi);
			ring[i].z = r * cos(phi);
			ring[i].points.resize(fragments);
			generate_circle(&ring[i].points.front(), rr, fragments);
		}

		p->append_poly();
		for (int i = 0; i < fragments; i++)
			p->append_vertex(ring[0].points[i].x, ring[0].points[i].y, ring[0].z);

		for (int i = 0; i < rings - 1; i++) {
			ring_s *r1 = &ring[i];
			ring_s *r2 = &ring[i + 1];
			int r1i = 0, r2i = 0;
			while (r1i < fragments || r2i < fragments)
			{
				if (r1i >= fragments)
					goto sphere_next_r2;
				if (r2i >= fragments)
					goto sphere_next_r1;
				if ((double)r1i / fragments <
					(double)r2i / fragments)
				{
				sphere_next_r1:
					p->append_poly();
					int r1j = (r1i + 1) % fragments;
					p->insert_vertex(r1->points[r1i].x, r1->points[r1i].y, r1->z);
					p->insert_vertex(r1->points[r1j].x, r1->points[r1j].y, r1->z);
					p->insert_vertex(r2->points[r2i % fragments].x, r2->points[r2i % fragments].y, r2->z);
					r1i++;
				}
				else {
				sphere_next_r2:
					p->append_poly();
					int r2j = (r2i + 1) % fragments;
					p->append_vertex(r2->points[r2i].x, r2->points[r2i].y, r2->z);
					p->append_vertex(r2->points[r2j].x, r2->points[r2j].y, r2->z);
					p->append_vertex(r1->points[r1i % fragments].x, r1->points[r1i % fragments].y, r1->z);
					r2i++;
				}
			}
		}

		p->append_poly();
		for (int i = 0; i < fragments; i++)
			p->insert_vertex(ring[rings - 1].points[i].x,
				ring[rings - 1].points[i].y,
				ring[rings - 1].z);
	}

	/*!
		Creates geometry for this node.
		May return an empty Geometry creation failed, but will not return NULL.
	*/
	ResultObject processChildren(const NodeGeometries &children) const override
	{
		Geometry *g = NULL;

		switch (this->type) {
		case CUBE: {
			PolySet *p = new PolySet(3, true);
			g = p;
			if (this->x > 0 && this->y > 0 && this->z > 0 &&
				!std::isinf(this->x) > 0 && !std::isinf(this->y) > 0 && !std::isinf(this->z) > 0) {
				double x1, x2, y1, y2, z1, z2;
				if (this->center) {
					x1 = -this->x / 2;
					x2 = +this->x / 2;
					y1 = -this->y / 2;
					y2 = +this->y / 2;
					z1 = -this->z / 2;
					z2 = +this->z / 2;
				}
				else {
					x1 = y1 = z1 = 0;
					x2 = this->x;
					y2 = this->y;
					z2 = this->z;
				}

				generateCube(x1, y1, z1, x2, y2, z2, *p);
			}
		}
				   break;
		case SPHERE: {
			PolySet *p = new PolySet(3, true);
			g = p;
			if (this->r1 > 0 && !std::isinf(this->r1))
				generateSphere(r1, fn, fs, fa, p);
		}
					 break;
		case CYLINDER: {
			PolySet *p = new PolySet(3, true);
			g = p;
			if (this->h > 0 && !std::isinf(this->h) &&
				this->r1 >= 0 && this->r2 >= 0 && (this->r1 > 0 || this->r2 > 0) &&
				!std::isinf(this->r1) && !std::isinf(this->r2)) {
				int fragments = Calc::get_fragments_from_r(std::fmax(this->r1, this->r2), this->fn, this->fs, this->fa);

				double z1, z2;
				if (this->center) {
					z1 = -this->h / 2;
					z2 = +this->h / 2;
				}
				else {
					z1 = 0;
					z2 = this->h;
				}

				point2d *circle1 = new point2d[fragments];
				point2d *circle2 = new point2d[fragments];

				generate_circle(circle1, r1, fragments);
				generate_circle(circle2, r2, fragments);

				for (int i = 0; i < fragments; i++) {
					int j = (i + 1) % fragments;
					if (r1 == r2) {
						p->append_poly();
						p->insert_vertex(circle1[i].x, circle1[i].y, z1);
						p->insert_vertex(circle2[i].x, circle2[i].y, z2);
						p->insert_vertex(circle2[j].x, circle2[j].y, z2);
						p->insert_vertex(circle1[j].x, circle1[j].y, z1);
					}
					else {
						if (r1 > 0) {
							p->append_poly();
							p->insert_vertex(circle1[i].x, circle1[i].y, z1);
							p->insert_vertex(circle2[i].x, circle2[i].y, z2);
							p->insert_vertex(circle1[j].x, circle1[j].y, z1);
						}
						if (r2 > 0) {
							p->append_poly();
							p->insert_vertex(circle2[i].x, circle2[i].y, z2);
							p->insert_vertex(circle2[j].x, circle2[j].y, z2);
							p->insert_vertex(circle1[j].x, circle1[j].y, z1);
						}
					}
				}

				if (this->r1 > 0) {
					p->append_poly();
					for (int i = 0; i < fragments; i++)
						p->insert_vertex(circle1[i].x, circle1[i].y, z1);
				}

				if (this->r2 > 0) {
					p->append_poly();
					for (int i = 0; i < fragments; i++)
						p->append_vertex(circle2[i].x, circle2[i].y, z2);
				}

				delete[] circle1;
				delete[] circle2;
			}
		}
					   break;
		case POLYHEDRON: {
			PolySet *p = new PolySet(3);
			g = p;
			p->setConvexity(this->convexity);
			for (size_t i = 0; i < this->faces->toVector().size(); i++)
			{
				Polygon poly;
				poly.open = this->open;
				const Value::VectorType &vec = this->faces->toVector()[i]->toVector();
				for (size_t j = 0; j < vec.size(); j++) {
					size_t pt = vec[j]->toDouble();
					if (pt < this->points->toVector().size()) {
						double px, py, pz;
						if (!this->points->toVector()[pt]->getVec3(px, py, pz) ||
							std::isinf(px) || std::isinf(py) || std::isinf(pz)) {
							PRINTB("ERROR: Unable to convert point at index %d to a vec3 of numbers", j);
							return ResultObject(p);
						}
						poly.push_back(Vector3d(px, py, pz));
					}
				}
				p->append_poly(poly);
			}
		}
						 break;
		case SQUARE: {
			Polygon2d *p = new Polygon2d();
			g = p;
			if (this->x > 0 && this->y > 0 &&
				!std::isinf(this->x) && !std::isinf(this->y)) {
				Vector2d v1(0, 0);
				Vector2d v2(this->x, this->y);
				if (this->center) {
					v1 -= Vector2d(this->x / 2, this->y / 2);
					v2 -= Vector2d(this->x / 2, this->y / 2);
				}

				Outline2d o;
				o.open = this->open;
				o.vertices.resize(4);
				o.vertices[0] = v1;
				o.vertices[1] = Vector2d(v2[0], v1[1]);
				o.vertices[2] = v2;
				o.vertices[3] = Vector2d(v1[0], v2[1]);
				p->addOutline(o);
			}
			p->setSanitized(true);
		}
					 break;
		case CIRCLE: {
			Polygon2d *p = new Polygon2d();
			g = p;
			if (this->r1 > 0 && !std::isinf(this->r1)) {
				int fragments = Calc::get_fragments_from_r(this->r1, this->fn, this->fs, this->fa);

				Outline2d o;
				o.open = this->open;
				o.vertices.resize(fragments);
				for (int i = 0; i < fragments; i++) {
					double phi = (M_PI * 2 * i) / fragments;
					o.vertices[i] = Vector2d(this->r1*cos(phi), this->r1*sin(phi));
				}
				p->addOutline(o);
			}
			p->setSanitized(true);
		}
					 break;
		case POLYGON: {
			Polygon2d *p = new Polygon2d();
			g = p;

			Outline2d outline;
			outline.open = this->open;
			double x, y;
			const Value::VectorType &vec = this->points->toVector();
			for (unsigned int i = 0; i < vec.size(); i++) {
				const Value &val = *vec[i];
				if (!val.getVec2(x, y) || std::isinf(x) || std::isinf(y)) {
					PRINTB("ERROR: Unable to convert point %s at index %d to a vec2 of numbers",
						val.toString() % i);
					return ResultObject(p);
				}
				outline.vertices.push_back(Vector2d(x, y));
			}

			if (this->paths->toVector().size() == 0 && outline.vertices.size() > 2) {
				p->addOutline(outline);
			}
			else {
				for (const auto &polygon : this->paths->toVector()) {
					Outline2d curroutline;
					curroutline.open = this->open;
					for (const auto &index : polygon->toVector()) {
						unsigned int idx = index->toDouble();
						if (idx < outline.vertices.size()) {
							curroutline.vertices.push_back(outline.vertices[idx]);
						}
						// FIXME: Warning on out of bounds?
					}
					p->addOutline(curroutline);
				}
			}

			if (p->outlines().size() > 0) {
				p->setConvexity(convexity);
			}
			break;
		}
		case LINE: {
			PolySet *p = new PolySet(3);
			g = p;
			p->setConvexity(this->convexity);
			Polygon poly;
			poly.open = true;
			int j = 0;
			for (auto &point : this->points->toVector()) {
				double px, py, pz;
				if (!point->getVec3(px, py, pz) ||
					std::isinf(px) || std::isinf(py) || std::isinf(pz)) {
					PRINTB("ERROR: Unable to convert point at index %d to a vec3 of numbers", j);
					return ResultObject(p);
				}
				poly.push_back(Vector3d(px, py, pz));
				++j;
			}
			p->append_poly(poly);
			break;
		} // LINE
		}

		return ResultObject(g);
	}
};

template <primitive_type_e PT>
class PrimitiveFactoryNode : public PrimitiveNode
{
public:
	template <typename ... Args>
	PrimitiveFactoryNode(Args ... args) : PrimitiveNode(PT, args...) { }
};

class CubeNode : public PrimitiveFactoryNode<primitive_type_e::CUBE>
{
public:
	CubeNode() : PrimitiveFactoryNode("size", "center") { }
	CubeNode(const BoundingBox &bb) : CubeNode()
	{
		auto size = bb.max() - bb.min();
		this->x = size[0];
		this->y = size[1];
		this->z = size[2];
		this->center = true;
	}
};
FactoryModule<CubeNode> CubeNodeFactory("cube");

class SphereNode : public PrimitiveFactoryNode<primitive_type_e::SPHERE>
{
public:
	SphereNode() : PrimitiveFactoryNode("r", "$fn", "$fs", "$fa") { }
};
FactoryModule<SphereNode> SphereNodeFactory("sphere");

class CylinderNode : public PrimitiveFactoryNode<primitive_type_e::CYLINDER>
{
public:
	CylinderNode() : PrimitiveFactoryNode("h", "r1", "r2", "center", "$fn", "$fs", "$fa") { }
};
FactoryModule<CylinderNode> CylinderNodeFactory("cylinder");

class PolyhedronNode : public PrimitiveFactoryNode<primitive_type_e::POLYHEDRON>
{
public:
	PolyhedronNode() : PrimitiveFactoryNode("points", "faces") { }
};
FactoryModule<PolyhedronNode> PolyhedronNodeFactory("polyhedron");

class SquareNode : public PrimitiveFactoryNode<primitive_type_e::SQUARE>
{
public:
	SquareNode() : PrimitiveFactoryNode("size", "center") { }
};
FactoryModule<SquareNode> SquareNodeFactory("square");

class CircleNode : public PrimitiveFactoryNode<primitive_type_e::CIRCLE>
{
public:
	CircleNode() : PrimitiveFactoryNode("r", "$fn", "$fs", "$fa") { }
};
FactoryModule<CircleNode> CircleNodeFactory("circle");

class PolygonNode : public PrimitiveFactoryNode<primitive_type_e::POLYGON>
{
public:
	PolygonNode() : PrimitiveFactoryNode("points", "paths", "open") { }
};
FactoryModule<PolygonNode> PolygonNodeFactory("polygon");

class LineNode : public PrimitiveFactoryNode<primitive_type_e::LINE>
{
public:
	LineNode() : PrimitiveFactoryNode("points") { }
};
FactoryModule<LineNode> LineNodeFactory("line");

// ---------------------------------------------------------------------
// BoundingBoxNode
// ---------------------------------------------------------------------
class BoundingBoxNode : public FactoryNode
{
public:
	BoundingBoxNode() : FactoryNode("delta")
		, delta(0, 0, 0)
	{
	}

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		auto delta = c.lookup("delta");
		delta->getDouble(this->delta[0]);
		delta->getDouble(this->delta[1]);
		delta->getDouble(this->delta[2]);
		delta->getVec3(this->delta[0], this->delta[1], this->delta[2]);
	}

	ResultObject processChildren(const NodeGeometries &children) const override
	{
		BoundingBox bb;
		for (auto &child : children)
			bb.extend(child.second->getBoundingBox());

		Vector3d min = bb.min() - delta;
		Vector3d max = bb.max() + delta;

		PolySet *ps = new PolySet(3);
		PrimitiveNode::generateCube(min, max, *ps);
		return ResultObject(ps);
	}

	Vector3d delta;
};

FactoryModule<BoundingBoxNode> BoundingBoxNodeFactory("boundingbox");

#include "cgal.h"
#include "CGAL_Nef_polyhedron.h"
#include "cgalutils.h"
#include "grid.h"
#include <CGAL/Min_sphere_of_spheres_d.h>

// ---------------------------------------------------------------------
// BoundingSphereNode
// ---------------------------------------------------------------------
class BoundingSphereNode : public FactoryNode
{
	typedef double											FT;
	typedef CGAL::Cartesian<FT>								K;
	typedef CGAL::Min_sphere_of_spheres_d_traits_3<K, FT>	Traits;
	typedef CGAL::Min_sphere_of_spheres_d<Traits>			Min_sphere;
	typedef K::Point_3										Point;
	typedef Traits::Sphere									Sphere;
public:
	BoundingSphereNode() : FactoryNode("delta", "$fn", "$fs", "$fa")
		, delta(0), fn(0), fs(0), fa(0)
	{
	}

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		auto delta = c.lookup("delta");
		delta->getDouble(this->delta);
		c.lookup("$fn")->getDouble(fn);
		c.lookup("$fs")->getDouble(fs);
		c.lookup("$fa")->getDouble(fa);
	}

	class SphereBuilder : public GeometryVisitor
	{
		Min_sphere bs;
	public:
		SphereBuilder() { }

		Vector3d getPoint(const Point &ep)
		{
			return Vector3d(ep[0], ep[1], ep[2]);
		}

		void addPoint(const Point &ep)
		{
			bs.insert(Sphere(ep, 0));
		}

		void addPoint(const CGAL_Kernel3::Point_3 &v)
		{
			Point ep(v[0].to_double(), v[1].to_double(), v[2].to_double());
			addPoint(ep);
		}

		void addPoint(const Vector3d &v)
		{
			Point ep(v[0], v[1], v[2]);
			addPoint(ep);
		}

		void addPoint(const Vector2d &v)
		{
			Point ep(v[0], v[1], 0);
			addPoint(ep);
		}

		double radius()
		{ 
			return bs.radius(); 
		}

		Vector3d center()
		{
			auto cc = bs.center_cartesian_begin();
			return Vector3d(cc[0], cc[1], cc[2]);
		}

		ResultObject visitChild(const ConstNefHandle &child) override
		{
			auto v0 = child->get()->vertices_begin();
			auto v1 = child->get()->vertices_end();
			for (; v0 != v1; ++v0)
				addPoint(v0->point());
			return ResultObject(nullptr);
		}

		ResultObject visitChild(const ConstPolySetHandle &child) override
		{
			for (auto &p : child->getPolygons()) {
				for (auto &v : p)
					addPoint(v);
			}
			return ResultObject(nullptr);
		}

		ResultObject visitChild(const Polygon2dHandle &child) override
		{
			for (auto &p : child->outlines()) {
				for (auto &v : p.vertices)
					addPoint(v);
			}
			return ResultObject(nullptr);
		}
	};

	ResultObject processChildren(const NodeGeometries &children) const override
	{
		SphereBuilder sb;
		sb.visitChildren(children);

		PolySet *ps = new PolySet(3);
		PrimitiveNode::generateSphere(sb.radius() + delta, fn, fs, fa, ps);
		ps->translate(sb.center());

		return ResultObject(ps);
	}

	double delta, fn, fs, fa;
};

FactoryModule<BoundingSphereNode> BoundingSphereNodeFactory("boundingsphere");
