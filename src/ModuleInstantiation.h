#pragma once

#include <vector>
#include "AST.h"
#include "localscope.h"
#include "Assignment.h"

typedef std::vector<class ModuleInstantiation*> ModuleInstantiationList;

class ModuleInstantiation : public ASTNode
{
public:
	ModuleInstantiation(const std::string &name, const AssignmentList &args = AssignmentList(), const Location &loc = Location::NONE)
		: ASTNode(loc), arguments(args), flags(NodeFlags::None), modname(name) { }
	ModuleInstantiation(const std::string &dotname, const std::string &name, const AssignmentList &args = AssignmentList(), const Location &loc = Location::NONE)
		: ASTNode(loc), arguments(args), flags(NodeFlags::None), dotname(dotname), modname(name) { }
	virtual ~ModuleInstantiation() { }

	virtual std::string dump(const std::string &indent) const;
	class AbstractNode *evaluate(const class Context *ctx) const;

	const std::string &name() const {
		return modname;
	}

	const std::string identifier() const { return dotname.empty() ? modname : dotname + "." + modname; }

	bool isBackground() const { return (this->flags & NodeFlags::Background) != 0; }
	bool isHighlight() const { return (this->flags & NodeFlags::Highlight) != 0; }
	bool isRoot() const { return (this->flags & NodeFlags::Root) != 0; }

	void setFlag(NodeFlags flag) {
		this->flags = (NodeFlags)(this->flags | flags);
	}

	AssignmentList arguments;
	LocalScope scope;
	NodeFlags flags;
protected:
	std::string dotname;
	std::string modname;
};

class IfElseModuleInstantiation : public ModuleInstantiation
{
public:
	IfElseModuleInstantiation(class Expression *expr, const Location &loc) 
		: ModuleInstantiation("if", AssignmentList{Assignment("", std::shared_ptr<Expression>(expr))}, loc) { }
	virtual ~IfElseModuleInstantiation() { }

	virtual std::string dump(const std::string &indent) const;

	LocalScope else_scope;
};