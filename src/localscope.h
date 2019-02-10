#pragma once

#include <unordered_map>
#include <vector>
#include "Handles.h"

struct NamedASTNode
{
	std::string name;
	std::shared_ptr<class ASTNode> node;

	NamedASTNode(const class Assignment &ass);
	NamedASTNode(std::string name, class UserStruct *s);
	NamedASTNode(std::string name, class UserFunction *f);
	NamedASTNode(std::string name, class UserModule *m);
	NamedASTNode(std::string name, class ModuleInstantiation *mi);
	NamedASTNode(std::string name, class Expression *e);
};

class LocalScope
{
public:
	size_t numElements() const { return orderedDefinitions.size(); }

	std::string dump(const std::string &indent) const;
	void print(std::ostream &stream) const;

	void addValue(const std::string &name, const class ValuePtr &value);
	void addAssignment(const class Assignment &ass);
	void addStruct(class UserStruct *astnode);
	void addFunction(class UserFunction *astnode);
	void addResult(class Expression *astnode);
	void addModule(class UserModule *astnode);
	void addChild(class ModuleInstantiation *astnode);

	void apply(class Context &ctx) const;
	void evaluate(class Context &ctx, NodeHandles &children) const;

	LocalScope operator+(const LocalScope &other) const;
	bool operator==(const LocalScope &other) const;

	// local assignments
	//std::vector<std::string> assignments;

	// local instantiations
	std::vector<ModuleInstantiation*> children;

	// shared pointers to user objects
	std::vector<NamedASTNode> orderedDefinitions;

	// functions for lookup
	typedef std::unordered_map<std::string, class AbstractFunction*> FunctionContainer;
	FunctionContainer functions;

	// modules for lookup
	typedef std::unordered_map<std::string, class AbstractModule*> ModuleContainer;
	ModuleContainer modules;
};
