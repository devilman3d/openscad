#include "ModuleInstantiation.h"
#include "evalcontext.h"
#include "expression.h"
#include "expressions.h"
#include "modcontext.h"
#include "node.h"

std::string ModuleInstantiation::dump(const std::string &indent) const
{
	std::stringstream dump;
	dump << indent;
	if (!dotname.empty())
		dump << dotname << ".";
	dump << modname + "(";
	for (size_t i=0; i < this->arguments.size(); i++) {
		const Assignment &arg = this->arguments[i];
		if (i > 0) dump << ", ";
		if (!arg.name.empty()) dump << arg.name << " = ";
		dump << *arg.expr;
	}
	if (scope.numElements() == 0) {
		dump << ");\n";
	} else if (scope.numElements() == 1) {
		dump << ") ";
		dump << scope.dump("");
	} else {
		dump << ") {\n";
		dump << scope.dump(indent + "\t");
		dump << indent << "}\n";
	}
	return dump.str();
}

std::string IfElseModuleInstantiation::dump(const std::string &indent) const
{
	std::stringstream dump;
	dump << ModuleInstantiation::dump(indent);
	dump << indent;
	if (else_scope.numElements() > 0) {
		dump << indent << "else ";
		if (else_scope.numElements() == 1) {
			dump << else_scope.dump("");
		}
		else {
			dump << "{\n";
			dump << else_scope.dump(indent + "\t");
			dump << indent << "}\n";
		}
	}
	return dump.str();
}

AbstractNode *ModuleInstantiation::evaluate(const Context *ctx) const
{
	ModuleContext ec(ctx, this);
	ec.setName("ModuleInstantiation", identifier());

#if DEBUG
	std::cerr << "Instantiating module: ";
	if (!dotname.empty())
		std::cerr << dotname << ".";
	std::cerr << modname << "\n";
#endif

	if (!dotname.empty()) {
		Lookup expr(dotname, location());
		auto v = expr.evaluate(ctx);
		if (v->isDefinedAs(Value::STRUCT)) {
			ScopeContext sc(ctx, v->toStruct());
			sc.setName("ModuleInstantiation", identifier());
			return sc.instantiate_module(&ec);
		}
		return nullptr;
	}

	return ctx->instantiate_module(&ec);
}
