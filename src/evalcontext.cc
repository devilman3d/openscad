#include "evalcontext.h"
#include "UserModule.h"
#include "ModuleInstantiation.h"
#include "expression.h"
#include "function.h"
#include "printutils.h"
#include "builtin.h"
#include "localscope.h"
#include "exceptions.h"

const std::string &EvalArguments::getArgName(size_t i) const
{
	assert(i < eval_arguments.size());
	return eval_arguments[i].name;
}

ValuePtr EvalArguments::getArgValue(size_t i, const Context *ctx) const
{
	assert(i < eval_arguments.size());
	const Assignment &arg = eval_arguments[i];
	ValuePtr v;
	if (arg.expr) {
		v = arg.expr->evaluate(ctx ? ctx : getEvalContext());
	}
	return v;
}

/*!
  Resolves arguments specified by evalctx, using args to lookup positional arguments.
  Returns an AssignmentMap (string -> Expression*)
*/
AssignmentMap EvalArguments::resolveArguments(const AssignmentList &args) const
{
  AssignmentMap resolvedArgs;
  size_t posarg = 0;
  // Iterate over positional args
  for (size_t i=0; i<this->numArgs(); i++) {
    const std::string &name = this->getArgName(i); // name is optional
    const Expression *expr = this->getArgs()[i].expr.get();
    if (!name.empty()) {
      resolvedArgs[name] = expr;
    }
    // If positional, find name of arg with this position
    else if (posarg < args.size()) resolvedArgs[args[posarg++].name] = expr;
  }
  return resolvedArgs;
}

EvalContext::EvalContext(const Context *parent, const AssignmentList &args)
	: Context(parent), EvalArguments(args)
{
	setType<EvalContext>();
}

void EvalContext::assignTo(Context &target) const
{
	for(const auto &assignment : eval_arguments) {
		if (!assignment.name.empty()) {
			ValuePtr v;
			if (assignment.expr) v = assignment.expr->evaluate(&target);
			if (target.has_local_variable(assignment.name)) {
				PRINTB("WARNING: Ignoring duplicate variable assignment %s = %s", assignment.name % v->toString());
			}
			else {
				target.set_variable(assignment.name, v);
			}
		}
	}
}

std::ostream &operator<<(std::ostream &stream, const EvalArguments &ec)
{
	for (size_t i = 0; i < ec.numArgs(); i++) {
		if (i > 0) stream << ", ";
		if (!ec.getArgName(i).empty()) stream << ec.getArgName(i) << " = ";
		ValuePtr val = ec.getArgValue(i, nullptr);
		stream << val->toEchoString();
	}
	return stream;
}

#ifdef DEBUG
std::string EvalContext::dump(const AbstractModule *mod, const ModuleInstantiation *inst)
{
	std::stringstream s;
	if (inst)
		s << boost::format("EvalContext %p (%p) for %s inst (%p)") % this % this->parent % inst->name() % inst;
	else
		s << boost::format("Context: %p (%p)") % this % this->parent;
	s << boost::format("  document path: %s") % this->document_path;

	s << boost::format("  eval args:");
	for (size_t i=0;i<this->eval_arguments.size();i++) {
		s << boost::format("    %s = %s") % this->eval_arguments[i].name % this->eval_arguments[i].expr;
	}
	if (this->scope && this->scope->children.size() > 0) {
		s << boost::format("    children:");
		for(const auto &ch : this->scope->children) {
			s << boost::format("      %s") % ch->name();
		}
	}
	if (mod) {
		const UserModule *m = dynamic_cast<const UserModule*>(mod);
		if (m) {
			s << boost::format("  module args:");
			for(const auto &arg : m->definition_arguments) {
				s << boost::format("    %s = %s") % arg.name % *(variables[arg.name]);
			}
		}
	}
	return s.str();
}
#endif
