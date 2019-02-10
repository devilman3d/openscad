#include "GeometryEvaluator.h"
#include "node.h"
#include "Tree.h"
#include "GeometryCache.h"
#include "CGALCache.h"
#include "Polygon2d.h"
#include "module.h"
#include "state.h"
#include "FactoryNode.h"
#include "transformnode.h"
#include "csgnode.h"
#include "csgops.h"
#include "CGAL_Nef_polyhedron.h"
#include "cgalutils.h"
#include "rendernode.h"
#include "clipper-utils.h"
#include "polyset-utils.h"
#include "polyset.h"
#include "calc.h"
#include "printutils.h"
#include "svg.h"
#include "calc.h"
#include "dxfdata.h"
#include "grid.h"
#include "PathHelpers.h"
#include "ModuleInstantiation.h"

#include <algorithm>

#include <CGAL/convex_hull_2.h>
#include <CGAL/Point_2.h>

const char *OP_NAMES[] = {
	"Union",
	"Intersection",
	"Difference",
	"Minkowski",
	"Glide",
	"Hull",
	"Resize",
	"Group"
};

// determines if kid is contained by parent
bool containsChild(const AbstractNode *parent, const AbstractNode *kid)
{
	if (parent == kid)
		return true;
	for (auto child : parent->getChildren())
		if (containsChild(child.get(), kid))
			return true;
	return false;
}

// gets the direct child of parent containing kid
const AbstractNode *childContaining(const AbstractNode *parent, const AbstractNode *kid)
{
	for (auto child : parent->getChildren())
		if (containsChild(child.get(), kid))
			return child.get();
	return NULL;
}

// compares the direct children of parent containing a and b
// returns true if the child indices are in order
// this is used via a lambda to std::sort in sortGeometries
bool compareAncestors(const AbstractNode *parent, const AbstractNode *a, const AbstractNode *b)
{
	auto ca = a;// childContaining(parent, a);
	auto cb = b;// childContaining(parent, b);
	if (ca->index() < cb->index())
		return true;
	return false;
}

// sorts children into result given a common parent node
void sortGeometries(const AbstractNode &parent, const NodeGeometries &children, NodeGeometries &result)
{
	// assign the result
	result.assign(children.begin(), children.end());
	// early out
	if (result.size() < 2)
		return;
	// sort the result
	auto comp = [&parent](const NodeGeometry &a, const NodeGeometry &b) -> bool
	{
		return compareAncestors(&parent, a.first, b.first);
	};
	std::sort(result.begin(), result.end(), comp);
}

GeometryHandle preferNef(const GeometryHandle &geom)
{
	if (auto ps = dynamic_pointer_cast<const PolySet>(geom)) {
		if (auto nef = CGALUtils::createNefPolyhedronFromGeometry(*ps))
			return GeometryHandle(nef);
		// TODO: error converting polyset to nef
		return nullptr;
	}
	if (auto gg = dynamic_pointer_cast<const GeometryGroup>(geom)) {
		NodeGeometries ng;
		for (auto &gc : gg->getChildren())
			if (auto gnef = preferNef(gc.second))
				ng.push_back(NodeGeometry(gc.first, gnef));
		return GeometryHandle(new GeometryGroup(ng));
	}
	return geom;
}

GeometryHandle preferPoly(const GeometryHandle &geom)
{
	if (auto nef = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(geom)) {
		if (auto ps = CGALUtils::createPolySetFromNefPolyhedron(*nef))
			return GeometryHandle(ps);
		// TODO: error converting nef to polyset
		return nullptr;
	}
	if (auto gg = dynamic_pointer_cast<const GeometryGroup>(geom)) {
		NodeGeometries ng;
		for (auto &gc : gg->getChildren())
			if (auto gnef = preferPoly(gc.second))
				ng.push_back(NodeGeometry(gc.first, gnef));
		return GeometryHandle(new GeometryGroup(ng));
	}
	return geom;
}

GeometryEvaluator::GeometryEvaluator(const class Tree &tree, Progress &progress, bool allownef, bool threaded /*=false*/)
	: ThreadedNodeVisitor(tree, progress, threaded)
	, tree(tree)
	, allowNef(allownef)
{
}

/*!
	Set allownef to false to force the result to _not_ be a Nef polyhedron
*/
shared_ptr<const Geometry> GeometryEvaluator::evaluateGeometry(const AbstractNode &node)
{
	std::string idString = this->tree.getIdString(node);
	shared_ptr<const Geometry> result;
	{
		boost::detail::spinlock::scoped_lock lock(cacheLock);
		IGeometryCache *primaryLookup = allowNef ? (IGeometryCache*)CGALCache::instance() : GeometryCache::instance();
		IGeometryCache *secondaryLookup = !allowNef ? (IGeometryCache*)CGALCache::instance() : GeometryCache::instance();
		if (primaryLookup->contains(idString))
			result = primaryLookup->get(idString);
		else if (secondaryLookup->contains(idString))
			result = secondaryLookup->get(idString);
	}

	// If not found in any caches, we need to evaluate the geometry
	if (!result) {
		this->root.reset();
		if (Feature::ExperimentalThreadedTraversal.is_enabled())
			this->traverseThreaded(node);
		else
			this->traverse(node);
		result = this->root;
	}

	// if NEFs aren't allowed, convert them to PolySets
	if (!allowNef)
		result = preferPoly(result);

	return result;
}

/*!
	Gets the processed children of the given node.
	When multithreaded, the children were processed in a non-deterministic order.
	They need to be sorted so difference, minkowski and others(?) work correctly.
*/
const NodeGeometries &GeometryEvaluator::getVisitedChildren(const AbstractNode &node)
{
	boost::detail::spinlock::scoped_lock lock(cacheLock);
	// check the sorted children first
	NodeGeometries &result = sortedchildren[node.index()];
	// if that's empty, sort the visited children into the sorted children
	if (result.empty())
		sortGeometries(node, visitedchildren[node.index()], result);
	return result;
}

//static Geometry *extrudePolygon(double height, const Polygon2d &poly, double convexity = 10);

/*!
	Since we can generate both Nef and non-Nef geometry, we need to insert it into
	the appropriate cache.
	This method inserts the geometry into the appropriate cache if it's not already cached.
*/
void GeometryEvaluator::smartCacheInsert(const AbstractNode &node,
	const shared_ptr<const Geometry> &geom)
{
	if (!geom)
		return;
	if (const auto gg = dynamic_pointer_cast<const GeometryGroup>(geom))
		return;
	if (ThreadedNodeVisitor::smartCacheInsert(node, geom))
		return;
	const std::string &key = this->tree.getIdString(node);
	return smartCacheInsert(key, geom);
}

void GeometryEvaluator::smartCacheInsert(const std::string &key,
	const shared_ptr<const Geometry> &geom)
{
	boost::detail::spinlock::scoped_lock lock(cacheLock);
	shared_ptr<const CGAL_Nef_polyhedron> N = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(geom);
	if (N) {
		//if (!CGALCache::instance()->contains(key)) {
		if (!CGALCache::instance()->insert(key, N)) {
			PRINT("WARNING: GeometryEvaluator: CGAL node did not fit into cache");
		}
		//}
		//else // pull any cached polyset geometry
		GeometryCache::instance()->remove(key);
	}
	else {
		//if (!GeometryCache::instance()->contains(key)) {
		if (!GeometryCache::instance()->insert(key, geom)) {
			PRINT("WARNING: GeometryEvaluator: Geometry node did not fit into cache");
		}
		//} 
		//else // pull any cached CGAL geometry
		CGALCache::instance()->remove(key);
	}
}

bool GeometryEvaluator::isSmartCached(const AbstractNode &node)
{
	shared_ptr<const Geometry> temp;
	if (ThreadedNodeVisitor::checkSmartCache(node, temp))
		return true;
	const std::string &key = this->tree.getIdString(node);
	return isSmartCached(key);
}

bool GeometryEvaluator::isSmartCached(const std::string &key)
{
	boost::detail::spinlock::scoped_lock lock(cacheLock);
	bool result = (GeometryCache::instance()->contains(key) ||
		CGALCache::instance()->contains(key));
	return result;
}

shared_ptr<const Geometry> GeometryEvaluator::smartCacheGet(const AbstractNode &node, bool preferNef)
{
	shared_ptr<const Geometry> temp;
	if (ThreadedNodeVisitor::checkSmartCache(node, temp))
		return temp;
	const std::string &key = this->tree.getIdString(node);
	return smartCacheGet(key, preferNef);
}

shared_ptr<const Geometry> GeometryEvaluator::smartCacheGet(const std::string &key, bool preferNef)
{
	boost::detail::spinlock::scoped_lock lock(cacheLock);
	shared_ptr<const Geometry> geom;
	bool hasgeom = GeometryCache::instance()->contains(key);
	bool hascgal = CGALCache::instance()->contains(key);
	if (hascgal && (preferNef || !hasgeom)) geom = CGALCache::instance()->get(key);
	else if (hasgeom) geom = GeometryCache::instance()->get(key);
	return geom;
}

bool GeometryEvaluator::checkSmartCache(const AbstractNode &node, bool preferNef, shared_ptr<const Geometry> &geom)
{
	if (ThreadedNodeVisitor::checkSmartCache(node, geom)) {
		return true;
	}
	const std::string &key = this->tree.getIdString(node);
	return checkSmartCache(key, preferNef, geom);
}

bool GeometryEvaluator::checkSmartCache(const std::string &key, bool preferNef, shared_ptr<const Geometry> &geom)
{
	boost::detail::spinlock::scoped_lock lock(cacheLock);
	bool hasgeom = GeometryCache::instance()->contains(key);
	bool hascgal = CGALCache::instance()->contains(key);
	if (hascgal && (preferNef || !hasgeom)) {
		geom = CGALCache::instance()->get(key);
		return true;
	}
	else if (hasgeom) {
		geom = GeometryCache::instance()->get(key);
		return true;
	}
	return false;
}

/*!
	Adds ourself to our parent's list of traversed children.
	Call this for _every_ node which affects output during traversal.
	Usually, this should be called from the postfix stage, but for some nodes,
	we defer traversal letting other components (e.g. CGAL) render the subgraph,
	and we'll then call this from prefix and prune further traversal.

	The added geometry can be NULL if it wasn't possible to evaluate it.
*/
void GeometryEvaluator::addToParent(const State &state,
	const AbstractNode &node,
	const shared_ptr<const Geometry> &g)
{
	// convert to the parent's preferred geometry type
	GeometryHandle geom =
		//state.parentPreferNef() ? preferNef(g) :
		//state.parentPreferPoly() ? preferPoly(g) :
		g;
	// add to the cache
	smartCacheInsert(node, geom);
	boost::detail::spinlock::scoped_lock lock(cacheLock);
	if (state.parent()) {
		auto *parent = state.parent();
		auto parentIndex = parent->index();
		if (false) {
			// erase the sorted children for this node's parent so they will be sorted when accessed
			this->sortedchildren.erase(parentIndex);
			// put this node's geometry pointer into its parent's geometry list
			if (geom)
				this->visitedchildren[parentIndex].push_back(NodeGeometry(&node, geom));
		}
		auto &sc = this->sortedchildren[parentIndex];
		if (sc.empty())
			sc.resize(parent->getChildren().size());
		sc[parent->indexOfChild(&node)] = NodeGeometry(&node, geom);
		// now, erase this node's copies of shared pointers
		this->visitedchildren.erase(node.index());
		this->sortedchildren.erase(node.index());
	}
	else {
		// Root node
		this->root = geom;
		// now, erase this node's copies of shared pointers
		this->visitedchildren.erase(node.index());
		this->sortedchildren.erase(node.index());
		// there shouldn't be any unvisited children!!!
		assert(this->visitedchildren.empty());
		assert(this->sortedchildren.empty());
	}
}

/*!
   Custom nodes are handled here => implicit union
*/
Response GeometryEvaluator::visit(State &state, const AbstractNode &node)
{
	if (state.isPrefix()) {
		if (isSmartCached(node)) return PruneTraversal;
		//state.setPreferNef(allowNef);	// default to Nefs or...
		//state.setPreferPoly(!allowNef); // ...PolySets
	}
	if (state.isPostfix()) {
		shared_ptr<const class Geometry> geom;
		if (!checkSmartCache(node, allowNef, geom)) {
			auto &gc = getVisitedChildren(node);
			if (!gc.empty()) {
				auto gg = GeometryHandle(new GeometryGroup(gc));
				geom = GeomUtils::simplify(gg);
			}
		}
		addToParent(state, node, geom);
	}
	return ContinueTraversal;
}

/*!
*/
Response GeometryEvaluator::visit(State &state, const GroupNode &node)
{
	return visit(state, (const AbstractNode &)node);
}

Response GeometryEvaluator::visit(State &state, const RootNode &node)
{
	// Just union the top-level objects
	return visit(state, (const GroupNode &)node);
}

/*!
	Leaf nodes can create their own geometry, so let them do that

	input: None
	output: PolySet or Polygon2d
*/
Response GeometryEvaluator::visit(State &state, const LeafNode &node)
{
	if (state.isPrefix()) {
		shared_ptr<const Geometry> geom;
		if (!checkSmartCache(node, allowNef, geom)) {
			auto geometry = node.createGeometry();
			assert(geometry);
			if (auto polygon = dynamic_pointer_cast<const Polygon2d>(geometry.constptr())) {
				if (!polygon->isSanitized()) {
					ClipperUtils utils;
					Polygon2d *p = utils.sanitize(*polygon);
					geometry.reset(p);
				}
			}
			geom = geometry.constptr();
		}
		addToParent(state, node, geom);
	}
	return PruneTraversal;
}

//static Geometry *extrudePolygon(double height, const Polygon2d &poly, double convexity)
//{
//	bool cvx = poly.is_convex();
//	PolySet *ps = new PolySet(3, !cvx ? boost::tribool(false) : boost::tribool(true));
//	ps->setConvexity(convexity);
//
//	if (height <= 0) return ps;
//
//	double h1, h2;
//
//	h1 = -height / 2.0;
//	h2 = +height / 2.0;
//
//	// bottom	
//	{
//		PolySet *ps_bottom = poly.tessellate();
//		// Flip vertex ordering for bottom polygon
//		for (Polygon &p : ps_bottom->polygons) {
//			std::reverse(p.begin(), p.end());
//		}
//		// translate bottom polyset
//		Vector3d originBot = Vector3d(0, 0, h1);
//		ps_bottom->translate(originBot);
//		ps->append(*ps_bottom);
//		delete ps_bottom;
//	}
//
//	// top
//	{
//		PolySet *ps_top = poly.tessellate();
//		// translate top polyset
//		Vector3d originTop = Vector3d(0, 0, h2);
//		ps_top->translate(originTop);
//		ps->append(*ps_top);
//		delete ps_top;
//	}
//
//	SliceSettings bot(h1);
//	SliceSettings top(h2);
//	SliceSettings::add_slice(ps, poly, bot, top);
//
//	return ps;
//}

/*!
	FIXME: Not in use
*/
Response GeometryEvaluator::visit(State &state, const AbstractPolyNode &node)
{
	assert(false);
	return AbortTraversal;
}

Response GeometryEvaluator::visit(State &state, const FactoryNode &node)
{
	if (state.isPrefix()) {
		if (isSmartCached(node)) return PruneTraversal;
		// this node type may consume Nefs or PolySets
		state.setPreferNef(node.preferNef());
		state.setPreferPoly(node.preferPoly());
	}
	if (state.isPostfix()) {
		shared_ptr<const Geometry> geom;
		if (!checkSmartCache(node, allowNef, geom)) {
			auto &gg = getVisitedChildren(node);
			if (auto temp = node.createGeometry(gg)) {
				if (temp.isConst())
					temp.reset(temp->copy());
				temp.ptr()->setConvexity(node.convexity);
				geom = temp.constptr();
			}
		}
		addToParent(state, node, geom);
	}
	return ContinueTraversal;
}

Response GeometryEvaluator::visit(State &state, const AbstractIntersectionNode &node)
{
	if (state.isPrefix()) {
		if (isSmartCached(node)) return PruneTraversal;
		state.setPreferNef(true); // this node type consumes Nefs
		state.setPreferPoly(false); // this node type consumes Nefs
	}
	if (state.isPostfix()) {
		shared_ptr<const class Geometry> geom;
		if (!checkSmartCache(node, allowNef, geom)) {
			auto &gg = getVisitedChildren(node);
			if (!gg.empty())
				if (auto temp = GeomUtils::apply(gg, OPENSCAD_INTERSECTION))
					geom = temp.constptr();
		}
		addToParent(state, node, geom);
	}
	return ContinueTraversal;
}
