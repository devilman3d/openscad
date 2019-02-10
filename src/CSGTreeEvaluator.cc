#include "CSGTreeEvaluator.h"
#include "state.h"
#include "csgops.h"
#include "module.h"
#include "ModuleInstantiation.h"
#include "csgnode.h"
#include "FactoryNode.h"
#include "transformnode.h"
#include "colornode.h"
#include "rendernode.h"
#include "printutils.h"
#include "GeometryEvaluator.h"
#include "polyset.h"
#include "polyset-utils.h"

#include <string>
#include <map>
#include <list>
#include <sstream>
#include <iostream>
#include <assert.h>
#include <cstddef>

/*!
	\class CSGTreeEvaluator

	A visitor responsible for creating a binary tree of CSGNode nodes used for rendering
	with OpenCSG.
*/

shared_ptr<CSGNode> CSGTreeEvaluator::buildCSGTree(const AbstractNode &node)
{
	this->traverse(node);
	
	shared_ptr<CSGNode> t(this->stored_term[node.index()]);
	if (t) {
		if (t->isHighlight())
			this->highlightNodes.push_back(t);
		if (t->isBackground()) {
			this->backgroundNodes.push_back(t);
			t.reset();
		}
	}

	return this->rootNode = t;
}

void CSGTreeEvaluator::applyBackgroundAndHighlight(State &state, const AbstractNode &node)
{
	for(const auto &chnode : this->visitedchildren[node.index()]) {
		shared_ptr<CSGNode> t(this->stored_term[chnode->index()]);
		this->stored_term.erase(chnode->index());
		if (t) {
			if (t->isBackground()) this->backgroundNodes.push_back(t);
			if (t->isHighlight()) this->highlightNodes.push_back(t);
		}
	}
}

void CSGTreeEvaluator::applyToChildren(State &state, const AbstractNode &node, OpenSCADOperator op)
{
	shared_ptr<CSGNode> t1;
	for(const auto &chnode : this->visitedchildren[node.index()]) {
		shared_ptr<CSGNode> t2(this->stored_term[chnode->index()]);
		//this->stored_term.erase(chnode->index());
		if (t2 && !t1) {
			t1 = t2;
		} else if (t2 && t1) {

			shared_ptr<CSGNode> t;
			// Handle background
			if (t1->isBackground() && 
					// For difference, we inherit the flag from the positive object
					(t2->isBackground() || op == OPENSCAD_DIFFERENCE)) {
				t = CSGOperation::createCSGNode(op, t1, t2);
				t->setBackground(true);
			}
			// Background objects are simply moved to backgroundNodes
			else if (t2->isBackground()) {
				t = t1;
				this->backgroundNodes.push_back(t2);
			}
			else if (t1->isBackground()) {
				t = t2;
				this->backgroundNodes.push_back(t1);
			}
			else {
				t = CSGOperation::createCSGNode(op, t1, t2);
			}
			// Handle highlight
				switch (op) {
				case OPENSCAD_DIFFERENCE:
					if (t != t1 && t1->isHighlight()) {
						t->setHighlight(true);
					}
					else if (t != t2 && t2->isHighlight()) {
						this->highlightNodes.push_back(t2);
					}
					break;
				case OPENSCAD_INTERSECTION:
					if (t && t != t1 && t != t2 &&
							t1->isHighlight() && t2->isHighlight()) {
						t->setHighlight(true);
					}
					else {
						if (t != t1 && t1->isHighlight()) {
							this->highlightNodes.push_back(t1);
						}
						if (t != t2 && t2->isHighlight()) {
							this->highlightNodes.push_back(t2);
						}
					}
					break;
				case OPENSCAD_UNION:
					if (t != t1 && t != t2 &&
							t1->isHighlight() && t2->isHighlight()) {
						t->setHighlight(true);
					}
					else if (t != t1 && t1->isHighlight()) {
						this->highlightNodes.push_back(t1);
						t = t2;
					}
					else if (t != t2 && t2->isHighlight()) {
						this->highlightNodes.push_back(t2);
						t = t1;
					}
					break;
				default:
					break;
				}
			t1 = t;
		}
	}
	if (t1) {
		if (node.isBackground()) t1->setBackground(true);
		if (node.isHighlight()) t1->setHighlight(true);
	}
	this->stored_term[node.index()] = t1;
}

Response CSGTreeEvaluator::visit(State &state, const AbstractNode &node)
{
	if (state.isPrefix()) {
		this->stored_state[node.index()] = state;
		return ContinueTraversal;
	}
	if (state.isPostfix()) {
		applyToChildren(state, node, OPENSCAD_UNION);
		addToParent(state, node);
	}
	return ContinueTraversal;
}

Response CSGTreeEvaluator::visit(State &state, const AbstractIntersectionNode &node)
{
	if (state.isPrefix()) {
		this->stored_state[node.index()] = state;
		return ContinueTraversal;
	}
	if (state.isPostfix()) {
		applyToChildren(state, node, OPENSCAD_INTERSECTION);
		addToParent(state, node);
	}
	return ContinueTraversal;
}

shared_ptr<CSGNode> CSGTreeEvaluator::evaluateCSGNodeFromGeometry(
	State &state, const shared_ptr<const Geometry> &geom, const AbstractNode &node)
{
	std::stringstream stream;
	stream << node.name() << node.index();

	// We cannot render Polygon2d directly, so we preprocess (tessellate) it here
	shared_ptr<const Geometry> g = geom;
	if (!g->isEmpty()) {
		if (auto p2d = dynamic_pointer_cast<const Polygon2d>(geom)) {
			g.reset(p2d->tessellate());
		}
		else if (auto gg = dynamic_pointer_cast<const GeometryGroup>(geom)) {
			assert(false && "Group???");
		}
		else if (auto ps = dynamic_pointer_cast<const PolySet>(geom))
		{
			// We cannot render concave polygons, so tessellate any 3D PolySets
			// Since is_convex() doesn't handle non-planar faces, we need to tessellate
			// also in the indeterminate state so we cannot just use a boolean comparison. See #1061
			assert(ps->getDimension() == 3);
			if (ps->poly_dim() > 3 && !ps->convexValue()) {
				PolySet *ps_tri = new PolySet(3, ps->convexValue());
				ps_tri->setConvexity(ps->getConvexity());
				PolysetUtils::tessellate_faces(*ps, *ps_tri);
				g.reset(ps_tri);
			}
		}
	}

	shared_ptr<CSGNode> t(new CSGLeaf(g, state.matrix(), state.color(), stream.str()));
	if (node.isHighlight()) 
		t->setHighlight(true);
	else if (node.isBackground()) 
		t->setBackground(true);
	return t;
}

Response CSGTreeEvaluator::visit(State &state, const AbstractPolyNode &node)
{
	if (state.isPrefix()) {
		this->stored_state[node.index()] = state;
		return ContinueTraversal;
	}
	if (state.isPostfix()) {
		shared_ptr<CSGNode> t1;
		if (this->geomevaluator) {
			shared_ptr<const Geometry> geom = this->geomevaluator->evaluateGeometry(node);
			if (geom) {
				t1 = evaluateCSGNodeFromGeometry(state, geom, node);
			}
			geomevaluator->getProgress().tick();
		}
		this->stored_term[node.index()] = t1;
		addToParent(state, node);
	}
	return ContinueTraversal;
}

Response CSGTreeEvaluator::visit(State &state, const CsgOpNode &node)
{
	if (state.isPrefix()) {
		this->stored_state[node.index()] = state;
		return ContinueTraversal;
	}
	if (state.isPostfix()) {
		applyToChildren(state, node, node.type);
		addToParent(state, node);
	}
	return ContinueTraversal;
}

Response CSGTreeEvaluator::visit(State &state, const TransformNode &node)
{
	if (state.isPrefix()) {
		if (matrix_contains_infinity(node.matrix) || matrix_contains_nan(node.matrix)) {
			PRINT("WARNING: Transformation matrix contains Not-a-Number and/or Infinity - removing object.");
			return PruneTraversal;
		}
		state.setMatrix(state.matrix() * node.matrix);
		this->stored_state[node.index()] = state;
		return ContinueTraversal;
	}
	if (state.isPostfix()) {
		applyToChildren(state, node, OPENSCAD_UNION);
		addToParent(state, node);
	}
	return ContinueTraversal;
}

Response CSGTreeEvaluator::visit(State &state, const ColorNode &node)
{
	if (state.isPrefix()) {
		if (!state.color().isValid()) state.setColor(node.color);
		this->stored_state[node.index()] = state;
		return ContinueTraversal;
	}
	if (state.isPostfix()) {
		applyToChildren(state, node, OPENSCAD_UNION);
		addToParent(state, node);
	}
	return ContinueTraversal;
}

Response CSGTreeEvaluator::visit(State &state, const BranchNode &node)
{
	if (state.isPrefix()) {
		this->stored_state[node.index()] = state;
		return ContinueTraversal;
	}
	if (state.isPostfix()) {
		if (this->geomevaluator) {
			shared_ptr<const Geometry> geom = this->geomevaluator->evaluateGeometry(node);
			if (geom) {
				if (auto gg = dynamic_pointer_cast<const GeometryGroup>(geom)) {
					for (auto &gi : gg->getChildren()) {
						const auto &childNode = *gi.first;
						auto childGeom = gi.second;
						auto &childState = stored_state[childNode.index()];
						auto term = this->stored_term[childNode.index()];
						if (auto leaf = dynamic_pointer_cast<CSGLeaf>(term)) {
							PRINTB("CSG: processor leaf: %s", leaf->dump());
							leaf->geom = childGeom;
							leaf->matrix = state.matrix();
							addToParent(state, childNode);
						}
						else if (term) {
							PRINTB("CSG: processor term: %s", term->dump());
							addToParent(state, childNode);
						}
						else {
							PRINTB("CSG: processor MISSING: %d", childNode.index());
							childState.setPostfix(true);
							if (childNode.accept(childState, *this) == AbortTraversal) {
								return AbortTraversal;
							}
							term = this->stored_term[childNode.index()];
							if (auto leaf = dynamic_pointer_cast<CSGLeaf>(term)) {
								PRINTB("....CSG: processor leaf: %s", leaf->dump());
								leaf->geom = childGeom;
								leaf->matrix = state.matrix();
								addToParent(state, childNode);
							}
							else if (term) {
								PRINTB("....CSG: processor term: %s", term->dump());
								addToParent(state, childNode);
							}
							else {
								PRINTB("....CSG: processor MISSING: %d", childNode.index());
							}
						}
						//applyBackgroundAndHighlight(state, *gi.first);
					}
					//applyToChildren(state, node, OPENSCAD_UNION);
					//addToParent(state, node);
				}
				else {
					auto t1 = evaluateCSGNodeFromGeometry(state, geom, node);
					this->stored_term[node.index()] = t1;
					applyBackgroundAndHighlight(state, node);
					addToParent(state, node);
				}
			}
			geomevaluator->getProgress().tick();
		}
	}
	return ContinueTraversal;
}

/*!
	Adds node to the parent state's list of traversed children.
	Call this for _every_ node which affects output during traversal.
    Usually, this should be called from the postfix stage, but for some nodes, we defer traversal letting other components (e.g. CGAL) render the subgraph, and we'll then call this from prefix and prune further traversal.
*/
void CSGTreeEvaluator::addToParent(const State &state, const AbstractNode &node)
{
	this->visitedchildren.erase(node.index());
	if (state.parent()) {
		this->visitedchildren[state.parent()->index()].push_back(&node);
	}
}
