#pragma once

#include "module.h"

class GroupModule : public AbstractModule
{
public:
	GroupModule() { }
	virtual ~GroupModule() { }
	virtual class AbstractNode *instantiate(const class Context *ctx, const class ModuleContext *evalctx) const;
};
