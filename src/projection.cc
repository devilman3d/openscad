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

#include "projectionnode.h"
#include "module.h"
#include "ModuleInstantiation.h"
#include "evalcontext.h"
#include "printutils.h"
#include "builtin.h"
#include "polyset.h"
#include "polyset-utils.h"
#include "clipper-utils.h"
#include "cgal.h"
#include "cgalutils.h"
#include "FactoryNode.h"

#include <assert.h>
#include <sstream>
#include <boost/assign/std/vector.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

class ProjectionNode : public FactoryNode
{
public:
	ProjectionNode() : FactoryNode("cut"), cut_mode(false) { }

	bool cut_mode;

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		ValuePtr cut = c.lookup_variable("cut", true);
		if (cut->type() == Value::BOOL)
			this->cut_mode = cut->toBool();
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		if (!this->cut_mode) {
			ClipperLib::Clipper sumclipper;
			for (const auto &item : children) {
				const AbstractNode *chnode = item.first;
				const shared_ptr<const Geometry> &chgeom = item.second;
				// FIXME: Don't use deep access to modinst members
				if (chnode->isBackground()) continue;

				const Polygon2d *poly = NULL;

				// CGAL version of Geometry projection
				// Causes crashes in createNefPolyhedronFromGeometry() for this model:
				// projection(cut=false) {
				//    cube(10);
				//    difference() {
				//      sphere(10);
				//      cylinder(h=30, r=5, center=true);
				//    }
				// }
#if 0
				shared_ptr<const PolySet> chPS = dynamic_pointer_cast<const PolySet>(chgeom);
				const PolySet *ps2d = NULL;
				shared_ptr<const CGAL_Nef_polyhedron> chN = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(chgeom);
				if (chN) chPS.reset(chN->convertToPolyset());
				if (chPS) ps2d = PolysetUtils::flatten(*chPS);
				if (ps2d) {
					CGAL_Nef_polyhedron *N2d = CGALUtils::createNefPolyhedronFromGeometry(*ps2d);
					poly = N2d->convertToPolygon2d();
				}
#endif

				// Clipper version of Geometry projection
				// Clipper doesn't handle meshes very well.
				// It's better in V6 but not quite there. FIXME: stand-alone example.
#if 1
					// project chgeom -> polygon2d
				shared_ptr<const PolySet> chPS = dynamic_pointer_cast<const PolySet>(chgeom);
				if (!chPS) {
					if (auto chN = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(chgeom)) {
						if (auto ps = CGALUtils::createPolySetFromNefPolyhedron(*chN))
							chPS.reset(ps);
						else
							PRINT("ERROR: Nef->PolySet failed");
					}
				}
				if (chPS) poly = PolysetUtils::project(*chPS);
#endif

				if (poly) {
					ClipperUtils utils;
					ClipperLib::Paths result = utils.fromPolygon2d(*poly);
					// Using NonZero ensures that we don't create holes from polygons sharing
					// edges since we're unioning a mesh
					result = utils.process(result,
						ClipperLib::ctUnion,
						ClipperLib::pftNonZero);
					// Add correctly winded polygons to the main clipper
					sumclipper.AddPaths(result, ClipperLib::ptSubject, true);
				}

				delete poly;
			}
			{
				ClipperLib::PolyTree sumresult;
				// This is key - without StrictlySimple, we tend to get self-intersecting results
				sumclipper.StrictlySimple(true);
				sumclipper.Execute(ClipperLib::ctUnion, sumresult, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
				if (sumresult.Total() > 0) {
					ClipperUtils utils;
					return ResultObject(utils.toPolygon2d(sumresult));
				}
			}
		}
		else {
			int dim = 3;
			GeometryHandles dim3;
			GeomUtils::collect(children, dim3, dim);
			if (auto newgeom = shared_ptr<const Geometry>(CGALUtils::applyOperator(dim3, OPENSCAD_UNION))) {
				shared_ptr<const CGAL_Nef_polyhedron> Nptr = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(newgeom);
				if (!Nptr) {
					Nptr.reset(CGALUtils::createNefPolyhedronFromGeometry(*newgeom));
				}
				if (!Nptr->isEmpty()) {
					if (Polygon2d *poly = CGALUtils::project(*Nptr, this->cut_mode)) {
						poly->setConvexity(this->convexity);
						return ResultObject(poly);
					}
				}
			}
		}
		return ResultObject(new ErrorGeometry());
	}
};

FactoryModule<ProjectionNode> ProjectionNodeFactory("projection");

// =============================================
// SplitNode
// =============================================

class SplitNode : public FactoryNode
{
public:
	SplitNode() : FactoryNode("plane") { }
	virtual ~SplitNode() { }

	void initialize(Context &ctx, const ModuleContext &evalctx) override
	{
		auto plane = ctx.lookup("plane");
		if (!plane->getVec4(a, b, c, d)) {
			a = b = d = 0;
			c = 1;
		}
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		return visitChildren(children);
	}

	virtual ResultObject visitChild(const ConstNefHandle &child) const
	{
		if (!child->isEmpty()) {
			if (auto p3 = CGALUtils::split(*child, CGAL_Nef_polyhedron3::Plane_3(a, b, c, d))) {
				CGAL_Nef_polyhedron *poly = new CGAL_Nef_polyhedron(p3);
				poly->setConvexity(convexity);
				return ResultObject(poly);
			}
			return ResultObject(new ErrorGeometry());
		}
		return ResultObject(new EmptyGeometry());
	}

	virtual ResultObject visitChild(const ConstPolySetHandle &child) const
	{
		auto nef = shared_ptr<CGAL_Nef_polyhedron>(CGALUtils::createNefPolyhedronFromGeometry(*child));
		auto result = visitChild(nef);
		return result;
	}

	// variables for the plane equation
	double a, b, c, d;
};

FactoryModule<SplitNode> SplitNodeFactory("split");

// =============================================
// RProjectNode
// =============================================

class RProjectNode : public FactoryNode
{
public:
	RProjectNode() : FactoryNode("inner", "outer")
		, inner(0)
		, outer(1)
	{
	}

	void initialize(Context &ctx, const ModuleContext &evalctx) override
	{
		ctx.lookup("inner")->getFiniteDouble(this->inner);
		ctx.lookup("outer")->getFiniteDouble(this->outer);
	}

	virtual ResultObject visitChild(const ConstNefHandle &child) const
	{
		if (auto ps = CGALUtils::createPolySetFromNefPolyhedron(*child))
			return visitChild(PolySetHandle(ps));
		return ResultObject(new EmptyGeometry());
	}

	virtual ResultObject visitChild(const ConstPolySetHandle &child) const
	{
		return ResultObject(child);
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		return visitChildren(children);
	}

	double inner, outer;
};

FactoryModule<RProjectNode> RProjectNodeFactory("rproject");

