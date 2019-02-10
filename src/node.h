#pragma once

#include <vector>
#include <list>
#include <map>
#include <string>
#include <algorithm>
#include <atomic>
#include "memory.h"
#include "BaseVisitable.h"
#include "AST.h"
#include "maybe_const.h"
#include "Handles.h"

/*!  

	The node tree is the result of evaluation of a module instantiation
	tree.  Both the module tree and the node tree are regenerated from
	scratch for each compile.

 */
class AbstractNode : public BaseVisitable
{
	// FIXME: the idx_counter/idx is mostly (only?) for debugging.
	// We can hash on pointer value or smth. else.
  //  -> remove and
	// use smth. else to display node identifier in CSG tree output?
	static size_t idx_counter;   // Node instantiation index
public:
	VISITABLE();

	AbstractNode();
	virtual ~AbstractNode();
	virtual std::string toString() const;
	/*! The 'OpenSCAD name' of this node, defaults to classname, but can be 
	    overloaded to provide specialization for e.g. CSG nodes, primitive nodes etc.
	    Used for human-readable output. */
	virtual std::string name() const final { return nodeName; }

	NodeHandles &getChildren() { return this->children; }
	const NodeHandles &getChildren() const { return this->children; }
	size_t index() const { return this->idx; }

	size_t indexOfChild(const AbstractNode *child) const;

	static void resetIndexCounter() { idx_counter = 0; }

	bool isBackground() const { return (this->nodeFlags & NodeFlags::Background) != 0; }
	bool isHighlight() const { return (this->nodeFlags & NodeFlags::Highlight) != 0; }
	bool isRoot() const { return (this->nodeFlags & NodeFlags::Root) != 0; }

	virtual void addChild(const class Context &c, const NodeHandle &child)
	{
		//child->parent = this;
		children.push_back(child);
	}

	virtual void addChildren(const class Context &c, const NodeHandles &children)
	{
		for (auto &child : children)
			addChild(c, child);
	}

	//AbstractNode *parent;
	NodeFlags nodeFlags;
	int idx; // Node index (unique per tree)
	std::string nodeName;

	template <typename NodeType, typename ... Args>
	static NodeType *create(const std::string &name, NodeFlags flags, Args ... args)
	{
		NodeType *result = new NodeType(args...);
		result->nodeFlags = flags;
		result->nodeName = name;
		return result;
	}
protected:
	NodeHandles children;
};

class AbstractIntersectionNode : public AbstractNode
{
public:
	VISITABLE();

	static AbstractIntersectionNode *create(NodeFlags flags = NodeFlags::None)
	{ 
		return AbstractNode::create<AbstractIntersectionNode>("intersection", flags); 
	}
};

class AbstractPolyNode : public AbstractNode
{
public:
	VISITABLE();

	enum render_mode_e {
		RENDER_CGAL,
		RENDER_OPENCSG
	};
};

/*!
  Logically groups objects together. Used as a way of passing
	objects around without having to perform unions on them.
 */
class GroupNode final : public AbstractNode
{
public:
	VISITABLE();

	static GroupNode *create(NodeFlags flags = NodeFlags::None)
	{ 
		return AbstractNode::create<GroupNode>("group", flags); 
	}
};

/*!
	Only instantiated once, for the top-level file.
*/
class RootNode final : public AbstractNode
{
public:
	VISITABLE();

	static RootNode *create(NodeFlags flags = NodeFlags::None)
	{
		return AbstractNode::create<RootNode>("root", flags);
	}
};

/*
	Base class for nodes which consume [child] geometries to create new geometry.
*/
class BranchNode : public AbstractPolyNode
{
public:
	VISITABLE();

	virtual ResultObject createGeometry(const NodeGeometries &children) const = 0;
};

/*
	Base class for nodes which consume arguments to create geometry.
*/
class LeafNode : public AbstractPolyNode
{
public:
	VISITABLE();

	virtual ResultObject createGeometry() const = 0;
};

std::ostream &operator<<(std::ostream &stream, const AbstractNode &node);
const AbstractNode *find_root_tag(const AbstractNode *n);
