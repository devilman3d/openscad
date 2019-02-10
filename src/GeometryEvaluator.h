#pragma once

#include "ThreadedNodeVisitor.h"

#include <utility>
#include <list>
#include <vector>
#include <map>

#include "enums.h"
#include "memory.h"
#include "maybe_const.h"
#include "Geometry.h"
#include "state.h"
#include "Polygon2d.h"
#include "FactoryNode.h"
#include "Handles.h"

class GeometryEvaluator : public ThreadedNodeVisitor
{
public:
	GeometryEvaluator(const class Tree &tree, Progress &progress, bool allownef, bool threaded);
	virtual ~GeometryEvaluator() {}

	shared_ptr<const Geometry> evaluateGeometry(const AbstractNode &node);

	virtual Response visit(State &state, const AbstractNode &node);
	virtual Response visit(State &state, const AbstractIntersectionNode &node);
	virtual Response visit(State &state, const AbstractPolyNode &node);
	virtual Response visit(State &state, const GroupNode &node);
	virtual Response visit(State &state, const RootNode &node);
	virtual Response visit(State &state, const LeafNode &node);
	virtual Response visit(State &state, const FactoryNode &node);

	const Tree &getTree() const { return this->tree; }

private:
	typedef maybe_const<Geometry> ResultObject;

	void smartCacheInsert(const AbstractNode &node, const shared_ptr<const Geometry> &geom);
	shared_ptr<const Geometry> smartCacheGet(const AbstractNode &node, bool preferNef);
	bool checkSmartCache(const AbstractNode &node, bool preferNef, shared_ptr<const Geometry> &geom);
	bool isSmartCached(const AbstractNode &node);

	void smartCacheInsert(const std::string &key, const shared_ptr<const Geometry> &geom);
	shared_ptr<const Geometry> smartCacheGet(const std::string &key, bool preferNef);
	bool checkSmartCache(const std::string &key, bool preferNef, shared_ptr<const Geometry> &geom);
	bool isSmartCached(const std::string &key);

	void addToParent(const State &state, const AbstractNode &node, const shared_ptr<const Geometry> &geom);
	const NodeGeometries &getVisitedChildren(const AbstractNode &node);

	std::map<int, NodeGeometries> sortedchildren;
	std::map<int, NodeGeometries> visitedchildren;
	const Tree &tree;
	shared_ptr<const Geometry> root;
	bool allowNef;

public:
};
