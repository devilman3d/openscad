#pragma once

#include "node.h"
#include "Assignment.h"
#include "value.h"
#include "memory.h"
#include "FactoryModule.h"
#include "Handles.h"

class Context;
class EvalContext;
class ModuleContext;
class Geometry;
class GeometryGroup;
class CGAL_Nef_polyhedron;
class PolySet;
class Polygon2d;

class ConstGeometryVisitor
{
public:
	virtual ResultObject visitChild(const ConstNefHandle &child) const;
	virtual ResultObject visitChild(const ConstPolySetHandle &child) const;
	virtual ResultObject visitChild(const Polygon2dHandle &child) const;
	virtual ResultObject visitChild(const GeometryGroupHandle &child) const;
	virtual ResultObject visitChild(const GeometryHandle &child) const;
	virtual ResultObject visitChildren(const NodeGeometries &gg, class CpuProgress *progress = nullptr) const;
	void recurseChildren(const NodeGeometries &gg) const;
};

class GeometryVisitor : public ConstGeometryVisitor
{
public:
	virtual ResultObject visitChild(const ConstNefHandle &child);
	virtual ResultObject visitChild(const ConstPolySetHandle &child);
	virtual ResultObject visitChild(const Polygon2dHandle &child);
	virtual ResultObject visitChild(const GeometryGroupHandle &child);
	virtual ResultObject visitChild(const GeometryHandle &child);
	virtual ResultObject visitChildren(const NodeGeometries &gg, class CpuProgress *progress = nullptr);
	void recurseChildren(const NodeGeometries &gg);
};

class FactoryNode : public BranchNode, public ConstGeometryVisitor
{
public:
	VISITABLE();
	
protected:
	void addArg(const std::string &name)
	{
		definition_arguments.push_back(Assignment(name));
	}

	void addArgs() { } // to satisfy the variadic template

	template <typename A, typename ... Args>
	void addArgs(A current, Args ... remaining)
	{
		addArg(current);
		addArgs(remaining...);
	}

public:
	template <typename ... Args>
	FactoryNode(Args ... args)
		: convexity(0)
	{
		// add the expected arguments
		addArgs(args...);
	}
	virtual std::string toString() const;

	// Called by FactoryModule::instantiate to pass parameters to this node.
	// The default implementation sets up a context and evaluates the arguments 
	// specified at construction time. Then, initialize is called. Finally, the
	// evalctx's scope is evaluated to create child nodes.
	virtual void instantiate(const Context *c, const ModuleContext *evalctx) final;

	// Sets local variables before initialize is called. The default implementation
	// sets convexity, debug and _debug.
	virtual void setLocals(const Context &ctx);

	// Called by GeometryEvaluator::visit to process child geometries.
	ResultObject createGeometry(const NodeGeometries &children) const override final;

	virtual bool preferNef() const { return false; }
	virtual bool preferPoly() const { return false; }

	// all factory nodes support convexity
	int convexity;
	// all factory nodes support debug
	ValuePtr debug;
	// the value of $debug
	ValuePtr _debug;
protected:
	// Called by createGeometry to process child geometries.
	// Implement this method in derived classes to do something.
	virtual ResultObject processChildren(const NodeGeometries &children) const = 0;

	// The default implementation does nothing. Override it to store values 
	// from the Context before it goes away.
	virtual void initialize(Context &ctx, const ModuleContext &evalctx) { }

private:
	AssignmentList definition_arguments;

protected:
	std::string nodeStr;
};

class NefNode : public FactoryNode
{
public:
	bool preferNef() const override { return true; }

	ResultObject processChildren(const NodeGeometries &children) const override;
	ResultObject visitChild(const ConstPolySetHandle &child) const override;

	static NefNode *create(NodeFlags flags = NodeFlags::None)
	{
		return AbstractNode::create<NefNode>("nef", flags);
	}
};

class PolyNode : public FactoryNode
{
public:
	bool preferPoly() const override { return true; }

	ResultObject processChildren(const NodeGeometries &children) const override;
	ResultObject visitChild(const ConstNefHandle &child) const override;

	static PolyNode *create(NodeFlags flags = NodeFlags::None)
	{
		return AbstractNode::create<PolyNode>("polyset", flags);
	}
};
