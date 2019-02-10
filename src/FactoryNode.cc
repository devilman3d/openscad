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

#include "FactoryNode.h"
#include "context.h"
#include "evalcontext.h"
#include "Assignment.h"
#include "ModuleInstantiation.h"
#include "Geometry.h"
#include "value.h"
#include "progress.h"
#include "modcontext.h"

#include "cgal.h"
#include "cgalutils.h"
#include "CGAL_Nef_polyhedron.h"

std::string FactoryNode::toString() const
{
	return name() + "(" + nodeStr + ")";
}

void FactoryNode::instantiate(const Context *ctx, const ModuleContext *evalctx)
{
	// context for this node's local variables
	Context locals(ctx);
	locals.setName("FactoryNode<" + name() + ">", "locals");

	// setup the context
	locals.setDocumentPath(evalctx->documentPath());
	locals.setVariables(definition_arguments, evalctx);

	// set local variables
	setLocals(locals);

	// initialize derived classes
	initialize(locals, *evalctx);

	// generate the node's string
	this->nodeStr = locals.toString();

	// note: locals is still in scope so its config variables remain accesible via Context.ctx_stack

	// context for child evaluation = call/lookup stack
	// parent is ctx to hide locals and inherit config variables
	Context stack(ctx);
	stack.setName("FactoryNode<" + name() + ">", "stack");

	// create the child nodes
	NodeHandles children;
	evalctx->evaluate(stack, children);

	// add the child nodes
	addChildren(stack, children);
}

void FactoryNode::setLocals(const Context &c)
{
	// lookup common args explicitly via the context 
	auto convexity = c.lookup("convexity", true);
	if (convexity->isDefinedAs(Value::NUMBER)) {
		this->convexity = (int)convexity->toDouble();
	}
	else {
		convexity = c.lookup("$convexity", true);
		if (convexity->isDefinedAs(Value::NUMBER)) {
			this->convexity = (int)convexity->toDouble();
		}
	}
	// lookup the global debug value
	auto debug = c.lookup("$debug", true);
	// use if it is a boolean
	if (debug->isDefinedAs(Value::BOOL)) {
		this->debug = this->_debug = debug;
	}
	// lookup the node's debug value
	debug = c.lookup("debug", true);
	// use if it is defined
	if (debug->isDefined()) {
		this->debug = debug;
	}
}

ResultObject FactoryNode::createGeometry(const NodeGeometries &children) const
{
	bool isDebug = (_debug->isDefinedAs(Value::BOOL) && _debug->toBool()) || debug->isDefined();
	if (isDebug) {

		std::stringstream str;
		str << "DEBUG: ";
		//if (parent)
		//	str << "[" << "#" << parent->idx << "] <- ";
		str << "#" << idx << " ";
		if (this->debug->isDefinedAs(Value::BOOL)) {
			str << toString() << " {";
			for (int i = 0; i < this->children.size(); ++i) {
				if (i > 0)
					str << ", ";
				str << "#" << this->children[i]->idx;
			}
			str << "}";
		}
		else {
			str << this->name() << "() " << debug->toEchoString();
		}
		PRINT(str.str());
	}
	ResultObject processed = processChildren(children);
	// simplify groups with <=1 objects
	ResultObject result = GeomUtils::simplify(processed);
	return result;
}

// ------------------------------------------------------------------------
// ConstGeometryVisitor
// ------------------------------------------------------------------------
ResultObject ConstGeometryVisitor::visitChild(const ConstNefHandle &child) const 
{ 
	return ResultObject(child); 
}

ResultObject ConstGeometryVisitor::visitChild(const ConstPolySetHandle &child) const
{ 
	return ResultObject(child); 
}

ResultObject ConstGeometryVisitor::visitChild(const Polygon2dHandle &child) const
{ 
	return ResultObject(child); 
}

ResultObject ConstGeometryVisitor::visitChild(const GeometryGroupHandle &child) const
{ 
	return visitChildren(child->getChildren()); 
}

ResultObject ConstGeometryVisitor::visitChild(const GeometryHandle &child) const
{
	if (auto cc = dynamic_pointer_cast<const GeometryGroup>(child))
		return visitChild(cc);
	if (auto cc = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(child))
		return visitChild(cc);
	if (auto cc = dynamic_pointer_cast<const PolySet>(child))
		return visitChild(cc);
	if (auto cc = dynamic_pointer_cast<const Polygon2d>(child))
		return visitChild(cc);
	return ResultObject(child);
}

ResultObject ConstGeometryVisitor::visitChildren(const NodeGeometries &gg, CpuProgress *progress) const
{
	if (progress)
		progress->setCount(gg.size());
	NodeGeometries result;
	for (auto &child : gg)
	{
		if (auto pc = visitChild(child.second))
			result.push_back(NodeGeometry(child.first, pc.constptr()));
		if (progress)
			progress->tick();
	}
	return ResultObject(new GeometryGroup(result));
}

void ConstGeometryVisitor::recurseChildren(const NodeGeometries &gg) const
{
	for (auto &child : gg) {
		if (auto cc = dynamic_pointer_cast<const GeometryGroup>(child.second))
			recurseChildren(cc->getChildren());
		else
			visitChild(child.second);
	}
}

// ------------------------------------------------------------------------
// GeometryVisitor
// ------------------------------------------------------------------------
ResultObject GeometryVisitor::visitChild(const ConstNefHandle &child)
{
	return ResultObject(child);
}

ResultObject GeometryVisitor::visitChild(const ConstPolySetHandle &child) 
{
	return ResultObject(child);
}

ResultObject GeometryVisitor::visitChild(const Polygon2dHandle &child) 
{
	return ResultObject(child);
}

ResultObject GeometryVisitor::visitChild(const GeometryGroupHandle &child) 
{
	return visitChildren(child->getChildren());
}

ResultObject GeometryVisitor::visitChild(const GeometryHandle &child) 
{
	if (auto cc = dynamic_pointer_cast<const GeometryGroup>(child))
		return visitChild(cc);
	if (auto cc = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(child))
		return visitChild(cc);
	if (auto cc = dynamic_pointer_cast<const PolySet>(child))
		return visitChild(cc);
	if (auto cc = dynamic_pointer_cast<const Polygon2d>(child))
		return visitChild(cc);
	return ResultObject(child);
}

ResultObject GeometryVisitor::visitChildren(const NodeGeometries &gg, CpuProgress *progress) 
{
	if (progress)
		progress->setCount(gg.size());
	NodeGeometries result;
	for (auto &child : gg)
	{
		if (auto pc = visitChild(child.second))
			result.push_back(NodeGeometry(child.first, pc.constptr()));
		if (progress)
			progress->tick();
	}
	return ResultObject(new GeometryGroup(result));
}

void GeometryVisitor::recurseChildren(const NodeGeometries &gg)
{
	for (auto &child : gg) {
		if (auto cc = dynamic_pointer_cast<const GeometryGroup>(child.second))
			recurseChildren(cc->getChildren());
		else
			visitChild(child.second);
	}
}

// ------------------------------------------------------------------------
// NefNode
// ------------------------------------------------------------------------
ResultObject NefNode::processChildren(const NodeGeometries &children) const
{
	return visitChildren(children);
}

ResultObject NefNode::visitChild(const ConstPolySetHandle &child) const
{
	if (auto nef = CGALUtils::createNefPolyhedronFromGeometry(*child))
		return ResultObject(nef);
	return ResultObject(new EmptyGeometry());
}

// allow explicit construction of NefNode in code
FactoryModule<NefNode> NefNodeFactory("nef");

// ------------------------------------------------------------------------
// PolyNode
// ------------------------------------------------------------------------
ResultObject PolyNode::processChildren(const NodeGeometries &children) const
{
	return visitChildren(children);
}

ResultObject PolyNode::visitChild(const ConstNefHandle &child) const
{
	if (auto ps = CGALUtils::createPolySetFromNefPolyhedron(*child))
		return ResultObject(ps);
	return ResultObject(new EmptyGeometry());
}

// allow explicit construction of PolyNode in code
FactoryModule<PolyNode> PolyNodeFactory("polyset");
