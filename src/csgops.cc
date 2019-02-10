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

#include "csgops.h"

#include "evalcontext.h"
#include "module.h"
#include "ModuleInstantiation.h"
#include "csgnode.h"
#include "builtin.h"
#include "Geometry.h"
#include <sstream>
#include <assert.h>

#include "cgal.h"
#include "cgalutils.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

#include "PolyMesh.h"

#include "FactoryModule.h"
#include "FactoryNode.h"

template <OpenSCADOperator OP>
class CsgOpFactoryNode : public CsgOpNode
{
public:
	CsgOpFactoryNode() : CsgOpNode(OP) { }

	bool preferNef() const override { return true; }

	bool needsConversion(const NodeHandles &nodes) const
	{
		for (auto &node : nodes) {
			if (needsConversion(node))
				return true;
		}
		return false;
	}

	bool needsConversion(const NodeHandle &node) const
	{
		if (auto fn = dynamic_pointer_cast<FactoryNode>(node)) {
			if (fn->preferPoly())
				return true;
			if (fn->preferNef())
				return false;
		}
		return needsConversion(node->getChildren());
	}
	
	void addChild(const Context &c, const NodeHandle &child) override
	{
		if (needsConversion(child)) {
			auto nefNode = NefNode::create(this->nodeFlags);
			nefNode->addChild(c, child);
			nefNode->setLocals(c);
			CsgOpNode::addChild(c, NodeHandle(nefNode));
		}
		else
			CsgOpNode::addChild(c, child);
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		return GeomUtils::apply(children, this->type);
	}
};

class UnionNode : public CsgOpFactoryNode<OpenSCADOperator::OPENSCAD_UNION> {};
FactoryModule<UnionNode> UnionNodeFactory("union");

class DifferenceNode : public CsgOpFactoryNode<OpenSCADOperator::OPENSCAD_DIFFERENCE> {};
FactoryModule<DifferenceNode> DifferenceNodeFactory("difference");

class IntersectionNode : public CsgOpFactoryNode<OpenSCADOperator::OPENSCAD_INTERSECTION> {};
FactoryModule<IntersectionNode> IntersectionNodeFactory("intersection");

// -------------------------------------------------
// CGAL corefine operations
// -------------------------------------------------

namespace PMP = CGAL::Polygon_mesh_processing;

class CorefineUnionNode : public FactoryNode
{
public:
	CorefineUnionNode() : first(nullptr) { }

	virtual ResultObject visitChild(shared_ptr<const CGAL_Nef_polyhedron> nef) const
	{
		if (auto ps = CGALUtils::createPolySetFromNefPolyhedron(*nef))
			return visitChild(PolySetHandle(ps));
		return ResultObject(new EmptyGeometry());
	}

	virtual ResultObject visitChild(shared_ptr<const PolySet> ps) const
	{
		auto mesh = std::make_shared<PolyMesh>(*ps);
		// main object
		if (!first) {
			first = mesh;
			return ResultObject(first);
		}
		// difference objects
		PolyMesh::Mesh temp;
		if (PMP::corefine_and_compute_union(first->getMesh(), mesh->getMesh(), temp)) {
			auto ptemp = new PolyMesh(temp);
			first.reset(ptemp);
		}
		else
			PRINT("WARNING: Error computing corefine union");
		return ResultObject(first);
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		visitChildren(children);
		if (first)
			return ResultObject(new PolyMesh(*first));
		return ResultObject(new PolySet(3));
	}

	mutable shared_ptr<PolyMesh> first;
};

FactoryModule<CorefineUnionNode> CorefineUnionNodeFactory("cunion");

class CorefineDifferenceNode : public FactoryNode
{
public:
	CorefineDifferenceNode() : first(nullptr) { }

	virtual ResultObject visitChild(shared_ptr<const CGAL_Nef_polyhedron> nef) const
	{
		if (auto ps = CGALUtils::createPolySetFromNefPolyhedron(*nef))
			return visitChild(PolySetHandle(ps));
		return ResultObject(new EmptyGeometry());
	}

	virtual ResultObject visitChild(shared_ptr<const PolySet> ps) const
	{
		auto mesh = std::make_shared<PolyMesh>(*ps);
		mesh->validate();
		// main object
		if (first) {
			// difference objects
			if (CGAL::Polygon_mesh_processing::does_self_intersect(first->getMesh()))
				PRINT("WARNING: first mesh is self intersecting");
			if (CGAL::Polygon_mesh_processing::does_self_intersect(mesh->getMesh()))
				PRINT("WARNING: difference mesh is self intersecting");
			if (!CGAL::Polygon_mesh_processing::does_bound_a_volume(first->getMesh()))
				PRINT("WARNING: first mesh does not bound a volume");
			if (!CGAL::Polygon_mesh_processing::does_bound_a_volume(mesh->getMesh()))
				PRINT("WARNING: difference mesh does not bound a volume");
			PolyMesh::Mesh temp;
			if (PMP::corefine_and_compute_difference(first->getMesh(), mesh->getMesh(), temp)) {
				auto ptemp = new PolyMesh(temp);
				first.reset(ptemp);
			}
			else
				PRINT("WARNING: Error computing corefine difference");
		}
		else
			first = mesh;
		return ResultObject(first);
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		visitChildren(children);
		if (first)
			return ResultObject(new PolyMesh(*first));
		return ResultObject(new PolySet(3));
	}

	mutable shared_ptr<PolyMesh> first;
};

FactoryModule<CorefineDifferenceNode> CorefineDifferenceNodeFactory("cdifference");

class CorefineIntersectionNode : public FactoryNode
{
public:
	CorefineIntersectionNode() { }

	virtual ResultObject visitChild(shared_ptr<const CGAL_Nef_polyhedron> nef) const
	{
		if (auto ps = CGALUtils::createPolySetFromNefPolyhedron(*nef))
			return visitChild(PolySetHandle(ps));
		return ResultObject(new EmptyGeometry());
	}

	virtual ResultObject visitChild(shared_ptr<const PolySet> ps) const
	{
		auto mesh = std::make_shared<PolyMesh>(*ps);
		// main object
		if (!first) {
			first = mesh;
			return ResultObject(first);
		}
		// first intersected object
		if (!second) {
			second = mesh;
			return ResultObject(second);
		}
		// union subsequent intersected objects
		PolyMesh::Mesh temp;
		if (PMP::corefine_and_compute_union(second->getMesh(), mesh->getMesh(), temp)) {
			auto ptemp = new PolyMesh(temp);
			second.reset(ptemp);
		}
		else
			PRINT("WARNING: Error computing corefine union for intersection");
		return ResultObject(second);
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		visitChildren(children);
		if (!second) {
			return ResultObject(new PolyMesh(*first));
		}
		PolyMesh::Mesh result;
		if (PMP::corefine_and_compute_intersection(first->getMesh(), second->getMesh(), result)) {
			return ResultObject(new PolyMesh(result));
		}
		else
			PRINT("WARNING: Error computing corefine intersection");
		return ResultObject(new PolySet(3));
	}

	mutable shared_ptr<PolyMesh> first;
	mutable shared_ptr<PolyMesh> second;
};

FactoryModule<CorefineIntersectionNode> CorefineIntersectionNodeFactory("cintersection");
