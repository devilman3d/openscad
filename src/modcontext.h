#pragma once

#include "context.h"
#include "FileModule.h"

/*
	This holds the context for a LocalScope.
*/
class ScopeContext : public Context
{
protected:
	// protected pass-thru constructor for derived classes
	ScopeContext(const Context *parent) : Context(parent), functions_p(nullptr), modules_p(nullptr) { }
public:
	static std::string contextType() { return "ScopeContext"; }

	// ScopeContext from literal nullptr
	//explicit ScopeContext(std::nullptr_t) : Context(nullptr), functions_p(nullptr), modules_p(nullptr) { }

	ScopeContext(const Context *parent, const class LocalScope &scope, const AssignmentList &defArgs = AssignmentList(), const EvalArguments *evalctx = nullptr);

	const AbstractModule *findLocalModule(const std::string &name) const override;
	const AbstractFunction *findLocalFunction(const std::string &name) const override;

	const LocalScope::FunctionContainer *functions_p;
	const LocalScope::ModuleContainer *modules_p;

	// persists this ScopeContext's values and lookups into a LocalScope
	void persist(LocalScope &scope) const;
};

/*!
	This holds the context for a ModuleInstantiation.
	It also implements EvalArguments to handle incoming parameters.
*/
class ModuleContext : public ScopeContext, public EvalArguments
{
public:
	static std::string contextType() { return "ModuleContext"; }

	//explicit ModuleContext(std::nullptr_t) : ScopeContext(nullptr), EvalArguments(nullptr), inst(nullptr) { }

	ModuleContext(const Context *parent, const ModuleInstantiation *inst);
	virtual ~ModuleContext();

	// implements EvalArguments method: returns this
	const Context *getEvalContext() const override { return this; }

	const ModuleInstantiation *getModuleInstantiation() const { return inst; }

	// evaluates the module instantiation's scope into ctx and children
	void evaluate(Context &ctx, NodeHandles &children) const;

	const Location &location() const;
	std::string name() const;
	NodeFlags flags() const;
	size_t numChildren() const;
	ModuleInstantiation *getChild(size_t i) const;

#ifdef DEBUG
	virtual std::string dump(const class AbstractModule *mod, const ModuleInstantiation *inst);
#endif
private:
	const ModuleInstantiation *inst;

// Experimental code. See issue #399
//	void evaluateAssignments(const AssignmentList &assignments);
};

/*!
	This holds the context for a UserModule instantiation.
*/
class UserContext : public ScopeContext
{
	static std::vector<UserContext*> moduleStack;
public:
	static const UserContext* stack_element(int n) { return moduleStack[n]; };
	static int stack_size() { return moduleStack.size(); };
public:
	static std::string contextType() { return "UserContext"; }

	explicit UserContext(const Context *ctx, const UserModule *module, const ModuleContext *evalctx);
	virtual ~UserContext();

	const UserModule *getUserModule() const { return module; }
	const ModuleContext *getModuleContext() const { return evalctx; }

private:
	const UserModule *module;
	const ModuleContext *evalctx;
};

/*!
	This holds the context for a FileModule.
*/
class FileContext : public ScopeContext
{
public:
	static std::string contextType() { return "FileContext"; }

	FileContext(const Context *parent, const FileModule &module);

	const AbstractModule *findLocalModule(const std::string &name) const override;
	const AbstractFunction *findLocalFunction(const std::string &name) const override;

private:
	const FileModule::ModuleContainer *usedlibs_p;
};
