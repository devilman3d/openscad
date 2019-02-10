#include "builtin.h"
#include "function.h"
#include "module.h"
#include "expression.h"
#include "expressions.h"
#include "value.h"
#include "FactoryNode.h"
#include "FactoryModule.h"
#include "printutils.h"
#include "Geometry.h"

Builtins *Builtins::instance(bool erase)
{
	static Builtins *_instance(nullptr);
	if (erase) {
		if (_instance) {
			delete _instance;
			_instance = nullptr;
		}
	}
	else if (!_instance) {
		_instance = new Builtins();
		_instance->initialize();
	}
	return _instance;
}

void Builtins::init(const char *name, class AbstractModule *module)
{
#ifndef ENABLE_EXPERIMENTAL
	if (module->is_experimental()) {
		return;
	}
#endif
	instance()->modules[name] = module;
}

void Builtins::init(const char *name, class AbstractFunction *function)
{
#ifndef ENABLE_EXPERIMENTAL
	if (function->is_experimental()) {
		return;
	}
#endif
	instance()->functions[name] = function;
}

void Builtins::init(const char *name, const ValuePtr &value)
{
	instance()->addValue(name, value);
}

extern void register_builtin_functions();
extern void register_builtin_group();
extern void register_builtin_control();
extern void initialize_builtin_dxf_dim();

std::string Builtins::isDeprecated(const std::string &name)
{
	auto *b = instance();
	if (b->deprecations.find(name) != b->deprecations.end()) {
		return b->deprecations[name];
	}
	return std::string();
}

void Builtins::release()
{
	instance(true);
}

void Builtins::initialize()
{
	init("PI", ValuePtr(M_PI));

	init("$world", ValuePtr(Transform3d::Identity()));
	init("$invWorld", ValuePtr(Transform3d::Identity()));

	init("$fn", ValuePtr(0.0));
	init("$fs", ValuePtr(2.0));
	init("$fa", ValuePtr(12.0));
	init("$t", ValuePtr(0.0));

	Value::VectorType zero3;
	zero3.push_back(ValuePtr(0.0));
	zero3.push_back(ValuePtr(0.0));
	zero3.push_back(ValuePtr(0.0));
	ValuePtr zero3val(zero3);
	init("$vpt", zero3val);
	init("$vpr", zero3val);
	init("$vpd", ValuePtr(500));

	register_builtin_functions();
	initialize_builtin_dxf_dim();
	register_builtin_group();
	register_builtin_control();

	this->deprecations["dxf_linear_extrude"] = "linear_extrude()";
	this->deprecations["dxf_rotate_extrude"] = "rotate_extrude()";
	this->deprecations["assign"] = "a regular assignment";
}

std::string Builtins::getLexerKeywords(int index)
{
	auto *b = instance();
	std::stringstream str;
	switch (index) {
	case 1:
		for (const auto &f : b->functions)
			str << f.first << " ";
		break;
	case 3:
		for (const auto &m : b->modules)
			str << m.first << " ";
		break;
	}
	return str.str();
}

class HelpNode : public FactoryNode
{
public:
	HelpNode() : FactoryNode() { }

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		std::stringstream str;
		str << "OpenSCAD Builtin Functions:\n";
		for (const auto &f : Builtins::getGlobalScope().functions) {
			str << "    " << f.first << "\n";
		}
		str << "OpenSCAD Builtin Modules:\n";
		for (const auto &f : Builtins::getGlobalScope().modules)
			str << "    " << f.first << "\n";
		PRINT(str.str());

		// pass-thru children
		return ResultObject(new GeometryGroup(children));
	}
};

FactoryModule<HelpNode> HelpFactoryModule("help");
