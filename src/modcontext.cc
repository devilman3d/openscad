#define _USE_MATH_DEFINES  // M_PI
#include "math.h"

#include "modcontext.h"
#include "UserModule.h"
#include "ModuleInstantiation.h"
#include "expression.h"
#include "function.h"
#include "printutils.h"
#include "builtin.h"
#include "ModuleCache.h"
#include "expressions.h"
#include <cmath>

ScopeContext::ScopeContext(const Context *parent, const LocalScope &scope, const AssignmentList &defArgs /*= AssignmentList()*/, const EvalArguments *evalctx /*= nullptr*/)
	: ScopeContext(parent)
{
	setType<ScopeContext>();
	if (evalctx)
		setVariables(defArgs, evalctx);
	// FIXME: Don't access module members directly
	this->functions_p = &scope.functions;
	this->modules_p = &scope.modules;
	scope.apply(*this);
	// Experimental code. See issue #399
	//	evaluateAssignments(scope.assignments);
}

void ScopeContext::persist(LocalScope &scope) const
{
	for (auto &v : this->persist_variables)
		scope.addValue(v.first, v.second);
	for (auto &f : *this->functions_p) {
		if (auto uf = dynamic_cast<UserFunction*>(f.second))
			scope.addFunction(new UserFunction(*uf));
		//else
		//	scope.functions[f.first] = f.second;
	}
	for (auto &m : *this->modules_p) {
		if (auto um = dynamic_cast<UserModule*>(m.second))
			scope.addModule(new UserModule(*um));
		//else
		//	scope.modules[m.first] = m.second;
	}
}

const AbstractFunction *ScopeContext::findLocalFunction(const std::string &name) const
{
	if (this->functions_p) {
		auto found = this->functions_p->find(name);
		if (found != this->functions_p->end()) {
			AbstractFunction *f = found->second;
			if (!f->is_enabled()) {
				PRINTB("WARNING: Experimental builtin function '%s' is not enabled.", name);
				return NULL;
			}
			return f;
		}
	}
	return NULL;
}

const AbstractModule *ScopeContext::findLocalModule(const std::string &name) const
{
	if (this->modules_p) {
		auto found = this->modules_p->find(name);
		if (found != this->modules_p->end()) {
			AbstractModule *m = found->second;
			if (!m->is_enabled()) {
				PRINTB("WARNING: Experimental builtin module '%s' is not enabled.", name);
				return NULL;
			}
			std::string replacement = Builtins::isDeprecated(name);
			if (!replacement.empty()) {
				PRINT_DEPRECATION("The %s() module will be removed in future releases. Use %s instead.", name % replacement);
			}
			return m;
		}
	}
	return NULL;
}

ModuleContext::ModuleContext(const Context *parent, const ModuleInstantiation *inst)
	: ScopeContext(parent)
	, EvalArguments(inst->arguments)
	, inst(inst)
{
	setType<ModuleContext>();
}

ModuleContext::~ModuleContext()
{
}

void ModuleContext::evaluate(Context &evalctx, NodeHandles &children) const
{
	inst->scope.evaluate(evalctx, children);
}

const Location &ModuleContext::location() const
{
	return inst->location();
}

std::string ModuleContext::name() const
{
	return inst->name();
}

NodeFlags ModuleContext::flags() const
{
	return inst->flags;
}

size_t ModuleContext::numChildren() const 
{ 
	return inst->scope.numElements(); 
}

ModuleInstantiation *ModuleContext::getChild(size_t i) const 
{ 
	return inst->scope.children[i]; 
}

// Experimental code. See issue #399
#if 0
void ModuleContext::evaluateAssignments(const AssignmentList &assignments)
{
	// First, assign all simple variables
	std::list<std::string> undefined_vars;
 	for(const auto &ass : assignments) {
		ValuePtr tmpval = ass.second->evaluate(this);
		if (tmpval->isUndefined()) undefined_vars.push_back(ass.first);
 		else this->set_variable(ass.first, tmpval);
 	}

	// Variables which couldn't be evaluated in the first pass is attempted again,
  // to allow for initialization out of order

	std::unordered_map<std::string, Expression *> tmpass;
	for(const auto &ass : assignments) {
		tmpass[ass.first] = ass.second;
	}
		
	bool changed = true;
	while (changed) {
		changed = false;
		std::list<std::string>::iterator iter = undefined_vars.begin();
		while (iter != undefined_vars.end()) {
			std::list<std::string>::iterator curr = iter++;
			std::unordered_map<std::string, Expression *>::iterator found = tmpass.find(*curr);
			if (found != tmpass.end()) {
				const Expression *expr = found->second;
				ValuePtr tmpval = expr->evaluate(this);
				// FIXME: it's not enough to check for undefined;
				// we need to check for any undefined variable in the subexpression
				// For now, ignore this and revisit the validity and order of variable
				// assignments later
				if (!tmpval->isUndefined()) {
					changed = true;
					this->set_variable(*curr, tmpval);
					undefined_vars.erase(curr);
				}
			}
		}
	}
}
#endif

#ifdef DEBUG
std::string ModuleContext::dump(const AbstractModule *mod, const ModuleInstantiation *inst)
{
	std::stringstream s;
	if (inst)
		s << boost::format("ModuleContext %p (%p) for %s inst (%p) ") % this % this->parent % inst->name() % inst;
	else
		s << boost::format("ModuleContext: %p (%p)") % this % this->parent;
	s << boost::format("  document path: %s") % this->document_path;
	if (mod) {
		const UserModule *m = dynamic_cast<const UserModule*>(mod);
	 	if (m) {
			s << "  module args:";
			for(const auto &arg : m->definition_arguments) {
				s << boost::format("    %s = %s") % arg.name % variables[arg.name];
			}
		}
	}
	typedef std::pair<std::string, ValuePtr> ValueMapType;
	s << "  vars:";
	for(const auto &v : constants) {
		s << boost::format("    %s = %s") % v.first % v.second;
	}
	for(const auto &v : variables) {
		s << boost::format("    %s = %s") % v.first % v.second;
	}
	for(const auto &v : config_variables) {
		s << boost::format("    %s = %s") % v.first % v.second;
	}
	return s.str();
}
#endif

std::vector<UserContext*> UserContext::moduleStack;

UserContext::UserContext(const Context *ctx, const UserModule *module, const ModuleContext *evalctx)
	: ScopeContext(ctx)
	, module(module)
	, evalctx(evalctx)
{
	setType<UserContext>();
	moduleStack.push_back(this);
	setVariables(module->definition_arguments, evalctx);
	set_variable("$children", ValuePtr(double(evalctx->numChildren())));
	set_variable("$parent_modules", ValuePtr(double(moduleStack.size())));
	// FIXME: Don't access module members directly
	this->functions_p = &module->scope.functions;
	this->modules_p = &module->scope.modules;
	// don't init here...UserModule evaluates its own scope
	//initializeScope(module->scope);
}

UserContext::~UserContext()
{
	moduleStack.pop_back();
}

FileContext::FileContext(const Context *parent, const FileModule &module) 
	: ScopeContext(parent), usedlibs_p(nullptr)
{
	setType<FileContext>();
	if (!module.modulePath().empty())
		this->document_path = module.modulePath();
	// FIXME: Don't access module members directly
	this->functions_p = &module.scope.functions;
	this->modules_p = &module.scope.modules;
	this->usedlibs_p = &module.usedlibs;
	// don't init here...FileModule evaluates its own scope
	//initializeScope(module->scope);
}

const AbstractModule *FileContext::findLocalModule(const std::string &name) const
{
	for (const auto &m : *this->usedlibs_p) {
		FileModule *usedmod = ModuleCache::instance()->lookup(m);
		// usedmod is NULL if the library wasn't compiled (error or file-not-found)
		if (usedmod) {
			auto found = usedmod->scope.modules.find(name);
			if (found != usedmod->scope.modules.end()) {
				return found->second;
			}
		}
	}
	return ScopeContext::findLocalModule(name);
}

const AbstractFunction *FileContext::findLocalFunction(const std::string &name) const
{
	for (const auto &m : *this->usedlibs_p) {
		FileModule *usedmod = ModuleCache::instance()->lookup(m);
		// usedmod is NULL if the library wasn't compiled (error or file-not-found)
		if (usedmod) {
			auto found = usedmod->scope.functions.find(name);
			if (found != usedmod->scope.functions.end()) {
				return found->second;
			}
		}
	}
	return ScopeContext::findLocalFunction(name);
}
