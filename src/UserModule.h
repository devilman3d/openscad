#pragma once

#include <string>
#include <deque>

#include "module.h"
#include "localscope.h"
#include "Assignment.h"

class UserModule : public AbstractModule, public ASTNode
{
public:
	UserModule(const std::string &name, const AssignmentList &args, const Location &loc) : ASTNode(loc), name(name), definition_arguments(args) { }

	virtual AbstractNode *instantiate(const Context *ctx, const ModuleContext *evalctx) const;
	virtual std::string dump(const std::string &indent, const std::string &name) const;

	std::string name;
	AssignmentList definition_arguments;
	LocalScope scope;
};
