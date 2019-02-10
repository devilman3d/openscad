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

#include "cgaladvnode.h"
#include "module.h"
#include "ModuleInstantiation.h"
#include "evalcontext.h"
#include "builtin.h"
#include "polyset.h"
#include "cgalutils.h"
#include "FactoryNode.h"
#include "maybe_const.h"
#include "PathHelpers.h"
#include "Handles.h"
#include <CGAL/convex_hull_2.h>
#include <sstream>
#include <assert.h>
#include <boost/assign/std/vector.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

class MinkowskiNode : public FactoryNode
{
public:
	Geometry *applyMinkowski2D(const GeometryHandles &children) const
	{
		Polygon2ds raw_polys;
		GeomUtils::collect(children, raw_polys);
		if (!raw_polys.empty()) {
			ClipperUtils utils;
			return utils.applyMinkowski(raw_polys);
		}
		return nullptr;
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		int dim = 0;
		GeometryHandles geom;
		GeomUtils::collect(children, geom, dim, true);
		const Geometry *result = nullptr;
		if (dim == 2)
			result = applyMinkowski2D(geom);
		if (dim == 3)
			result = CGALUtils::applyMinkowski(geom);
		if (result)
			return ResultObject(result);
		return ResultObject(new EmptyGeometry());
	}
};

FactoryModule<MinkowskiNode> MinkowskiNodeFactory("minkowski");

class GlideNode : public FactoryNode
{
public:
	ValuePtr paths;
	ValuePtr points;

	GlideNode() : FactoryNode("points", "paths") { }

	void initialize(Context &ctx, const ModuleContext &evalctx) override
	{
		paths = ctx.lookup("paths");
		points = ctx.lookup("points");
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		GeometryHandles paths;

		// create polyhedron(s) for the path/points[path]/points[paths[path]]
		{
			auto polys = PathHelpers::createPolylines3d(this->paths, this->points);
			for (auto poly : polys)
			{
				PRINT("Glide: Processing path");
				GeometryHandle pp(new CGAL_Nef_polyhedron(*poly));
				paths.push_back(pp);
				delete poly;
			}
		}

		// find all the non-empty children
		int dim = 3;
		GeometryHandles actualchildren;
		GeomUtils::collect(children, actualchildren, dim);

		GeometryHandles finishchildren;
		if (!paths.empty() && !actualchildren.empty())
		{
			size_t t = paths.size();
			auto iter = paths.begin();
			for (size_t i = 1; iter != paths.end(); ++i, ++iter)
			{
				GeometryHandles pathChildren = actualchildren;
				pathChildren.insert(pathChildren.begin(), *iter);
				// apply the glide
				try
				{
					PRINTB("Glide: Performing Minkowski on path %d/%d", i % t);
					GeometryHandle mink(CGALUtils::applyMinkowski(pathChildren));
					finishchildren.push_back(mink);
					PRINTB("Glide: Finished Minkowski on path %d/%d", i % t);
				}
				catch (const std::exception &ex)
				{
					PRINTB("Glide: Caught an exeption: %s", ex.what());
				}
			}
		}
		else if (!actualchildren.empty())
		{
			GeometryHandles justChildren = actualchildren;
			if (justChildren.size() > 1)
			{
				// apply the glide
				PRINT("Glide: Performing Minkowski");
				GeometryHandle mink(CGALUtils::applyMinkowski(justChildren));
				finishchildren.push_back(mink);
			}
			else if (justChildren.size() == 1)
				finishchildren.push_back(justChildren.front());
		}

		// nothing to do?
		if (finishchildren.empty()) return ResultObject(new EmptyGeometry());

		// this is a noop?
		if (finishchildren.size() == 1) return ResultObject(finishchildren.front());

		// union the result
		PRINT("Glide: Unioning result");
		return ResultObject(CGALUtils::applyOperator(finishchildren, OPENSCAD_UNION));
	}
};

FactoryModule<GlideNode> GlideNodeFactory("glide");

// TODO: implement SubdivNode

class HullNode : public FactoryNode
{
public:
	Polygon2d *applyHull2D(const GeometryHandles &children) const
	{
		Polygon2d *geometry = new Polygon2d();

		typedef CGAL::Point_2<CGAL::Cartesian<double>> CGALPoint2;
		// Collect point cloud
		std::list<CGALPoint2> points;
		for (const auto &p : children) {
			if (auto p2d = dynamic_pointer_cast<const Polygon2d>(p)) {
				for (const auto &o : p2d->outlines()) {
					for (const auto &v : o.vertices) {
						points.push_back(CGALPoint2(v[0], v[1]));
					}
				}
			}
		}
		if (points.size() > 0) {
			// Apply hull
			std::list<CGALPoint2> result;
			CGAL::convex_hull_2(points.begin(), points.end(), std::back_inserter(result));

			// Construct Polygon2d
			Outline2d outline;
			for (const auto &p : result) {
				outline.vertices.push_back(Vector2d(p[0], p[1]));
			}
			geometry->addOutline(outline);
		}
		return geometry;
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		int dim = 0;
		GeometryHandles geom;
		GeomUtils::collect(children, geom, dim);
		const Geometry *result = nullptr;
		if (dim == 2)
			result = applyHull2D(geom);
		if (dim == 3) {
			auto ps = new PolySet(3);
			if (CGALUtils::applyHull(geom, *ps))
				result = ps;
		}
		if (result)
			return ResultObject(result);
		return ResultObject(new EmptyGeometry());
	}
};

FactoryModule<HullNode> HullNodeFactory("hull");

class ResizeNode : public FactoryNode
{
public:
	Vector3d newsize;
	Eigen::Matrix<bool, 3, 1> autosize;

	ResizeNode() : FactoryNode("newsize", "auto") { }

	void initialize(Context &ctx, const ModuleContext &evalctx) override
	{
		ValuePtr ns = ctx.lookup("newsize");
		this->newsize << 0, 0, 0;
		if (ns->type() == Value::VECTOR) {
			const Value::VectorType &vs = ns->toVector();
			if (vs.size() >= 1) this->newsize[0] = vs[0]->toDouble();
			if (vs.size() >= 2) this->newsize[1] = vs[1]->toDouble();
			if (vs.size() >= 3) this->newsize[2] = vs[2]->toDouble();
		}
		ValuePtr autosize = ctx.lookup("auto");
		this->autosize << false, false, false;
		if (autosize->type() == Value::VECTOR) {
			const Value::VectorType &va = autosize->toVector();
			if (va.size() >= 1) this->autosize[0] = va[0]->toBool();
			if (va.size() >= 2) this->autosize[1] = va[1]->toBool();
			if (va.size() >= 3) this->autosize[2] = va[2]->toBool();
		}
		else if (autosize->type() == Value::BOOL) {
			this->autosize << autosize->toBool(), autosize->toBool(), autosize->toBool();
		}
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		if (auto res = GeomUtils::apply(children, OPENSCAD_UNION)) {
			if (res.isConst())
				res.reset(res->copy());
			auto editable = res.ptr();
			if (auto N = dynamic_pointer_cast<CGAL_Nef_polyhedron>(editable)) {
				N->resize(this->newsize, this->autosize);
			}
			else if (auto poly = dynamic_pointer_cast<Polygon2d>(editable)) {
				poly->resize(Vector2d(this->newsize[0], this->newsize[1]),
					Eigen::Matrix<bool, 2, 1>(this->autosize[0], this->autosize[1]));
			}
			else if (auto ps = dynamic_pointer_cast<PolySet>(editable)) {
				ps->resize(this->newsize, this->autosize);
			}
			else {
				assert(false);
			}
			return res;
		}
		return ResultObject(new EmptyGeometry());
	}
};

FactoryModule<ResizeNode> ResizeNodeFactory("resize");
