#pragma once

#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include "value.h"
#include "Assignment.h"
#include "memory.h"

class Context
{
	// prevent coding errors - Context is not copy-constructable
	Context(const Context &copy) = delete;
public:
	typedef std::vector<const Context*> Stack;

	Context(std::nullptr_t) noexcept : parent(nullptr), ctx_stack(nullptr) { }

	Context(const Context *parent = nullptr);
	virtual ~Context();

	static std::string contextType() { return "Context"; }

	template <typename T>
	void setType()
	{
		this->typeName = T::contextType();
	}

	void setName(const std::string &name, const std::string &what)
	{
		this->name = name;
		this->what = what;
	}

	const Context *getParent() const { return this->parent; }

	virtual ValuePtr evaluate_function(const std::string &name, const class EvalContext *evalctx) const;
	virtual class AbstractNode *instantiate_module(const class ModuleContext *evalctx) const;

	virtual const AbstractModule *findLocalModule(const std::string &name) const { return nullptr; }
	virtual const AbstractFunction *findLocalFunction(const std::string &name) const { return nullptr; }

	void setVariables(const AssignmentList &args, const class EvalArguments *evalargs);

	void set_variable(const std::string &name, const ValuePtr &value, bool persistent = true);
	void set_variable(const std::string &name, const Value &value, bool persistent = true);

	void apply_variables(const Context &other);
	ValuePtr lookup_variable(const std::string &name, bool silent = false) const;
	ValuePtr lookup(const std::string &name, bool silent = false) const;
	bool has_local_variable(const std::string &name) const;

	void setDocumentPath(const std::string &path) { this->document_path = path; }
	const std::string &documentPath() const { return this->document_path; }
	std::string getAbsolutePath(const std::string &filename) const;
        
public:
	std::string toString() const;

protected:
	std::string typeName;
	std::string name;
	std::string what;
	const Context *parent;
	Stack *ctx_stack;

	typedef std::map<std::string, ValuePtr> ValueMap;
	ValueMap variables;
	ValueMap config_variables;
	ValueMap persist_variables;

	std::string document_path; // FIXME: This is a remnant only needed by dxfdim

public:
#ifdef DEBUG
	virtual std::string dump(const class AbstractModule *mod, const ModuleInstantiation *inst);
#endif
};
