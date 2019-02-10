#pragma once

#include "node.h"
#include "module.h"
#include "modcontext.h"

/*
	A "factory" AbstractModule base class for the typed FactoryModule class.
	Automatically registers with Builtins.
	See also: FactoryNode
*/
class FactoryModuleBase : public AbstractModule
{
public:
	FactoryModuleBase(const char *name);

	// implement AbstractModule::instantiate
	virtual AbstractNode *instantiate(const Context *ctx, const ModuleContext *evalctx) const;

	// pure virtual to be implemented by FactoryModule
	virtual class FactoryNode *createNode(NodeFlags flags) const = 0;

	std::string nodeName;
};

/*
	A templated FactoryModuleBase for a given node type.
	A single static instance should be declared for each node type, 
	i.e. specify this in the NodeType's .cc file:
	  FactoryModule<NodeType> NodeTypeFactory("nodename");
	NodeType must be a FactoryNode.
	See also: FactoryNode
*/
template <typename NodeType>
class FactoryModule : public FactoryModuleBase
{
public:
	FactoryModule(const char *name) : FactoryModuleBase(name) { }

	class FactoryNode *createNode(NodeFlags flags) const override final
	{
		return AbstractNode::create<NodeType>(nodeName, flags);
	}
};
