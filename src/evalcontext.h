#pragma once

#include "context.h"
#include "Assignment.h"

/*!
  This holds the parameters actually sent when calling a module or function.
  It serves as a [secondary] base class for those contexts.
*/
class EvalArguments
{
protected:
	EvalArguments(std::nullptr_t) { }

public:
	explicit EvalArguments(const AssignmentList &a) : eval_arguments(a) { }
	virtual ~EvalArguments() { }

	virtual const Context *getEvalContext() const = 0;

	size_t numArgs() const { return eval_arguments.size(); }
	const std::string &getArgName(size_t i) const;
	ValuePtr getArgValue(size_t i, const Context *ctx = nullptr) const;
	const AssignmentList & getArgs() const { return eval_arguments; }

	AssignmentMap resolveArguments(const AssignmentList &args) const;

	AssignmentList eval_arguments;
};

/*!
  This holds the context for a function call or evaluation.
*/
class EvalContext : public Context, public EvalArguments
{
public:
	static std::string contextType() { return "EvalContext"; }

	EvalContext(const Context *parent, const AssignmentList &args);
	virtual ~EvalContext() { }

	const Context *getEvalContext() const override { return this; }

	void assignTo(Context &target) const;

#ifdef DEBUG
	virtual std::string dump(const class AbstractModule *mod, const ModuleInstantiation *inst);
#endif
};

std::ostream &operator<<(std::ostream &stream, const EvalArguments &ec);
