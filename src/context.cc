/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "context.h"
#include "evalcontext.h"
#include "modcontext.h"
#include "expression.h"
#include "function.h"
#include "UserModule.h"
#include "ModuleInstantiation.h"
#include "builtin.h"
#include "printutils.h"
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

// $children is not a config_variable. config_variables have dynamic scope, 
// meaning they are passed down the call chain implicitly.
// $children is simply misnamed and shouldn't have included the '$'.
static bool is_config_variable(const std::string &name) {
	return name[0] == '$' && name != "$children";
}

/**
 * This is separated because PRINTB uses quite a lot of stack space
 * and the methods using it evaluate_function() and instantiate_module()
 * are called often when recursive functions or modules are evaluated.
 *
 * @param what what is ignored
 * @param name name of the ignored object
 */
static void print_ignore_warning(const char *what, const char *name)
{
	PRINTB("WARNING: Ignoring unknown %s '%s'.", what % name);
}

/*!
	Initializes this context. Optionally initializes a context for an 
	external library. Note that if parent is null, a new stack will be
	created, and all children will share the root parent's stack.
*/
Context::Context(const Context *parent)
	: parent(parent)
{
	setType<Context>();
	if (parent) {
		assert(parent->ctx_stack && "Parent context stack was null!");
		this->ctx_stack = parent->ctx_stack;
		this->document_path = parent->document_path;
	}
	else {
		this->ctx_stack = new Stack;
	}

	this->ctx_stack->push_back(this);
}

Context::~Context()
{
	assert(this->ctx_stack && "Context stack is null at destruction!");
	assert(this->ctx_stack->size() > 0 && "Context stack is empty at destruction!");
	this->ctx_stack->pop_back();
	if (!parent) delete this->ctx_stack;
}

/*!
	Initialize context from a "default" argument list and evaluation arguments.
*/
void Context::setVariables(const AssignmentList &args, const EvalArguments *evalargs)
{
	// evaluate any default values in the parent context
	for (const auto &arg : args)
		set_variable(arg.name, arg.expr ? arg.expr->evaluate(this->parent) : ValuePtr::undefined);

	if (evalargs) {
		// resolve named/unnamed incoming expressions to the default names
		AssignmentMap assignments = evalargs->resolveArguments(args);
		// evaluate resolved expressions via the eval context
		for (const auto &ass : assignments) {
			set_variable(ass.first, ass.second->evaluate(evalargs->getEvalContext()));
		}
	}
}

void Context::set_variable(const std::string &name, const ValuePtr &value, bool persistent /*= true*/)
{
	if (is_config_variable(name)) {
		// don't undef config variables
		if (value->isDefined())
			config_variables[name] = value;
	}
	else {
		// allow regular variables to be undef so lookup_variable doesn't warn about optional arguments
		variables[name] = value;
	}
	if (persistent) {
		// store for persistence
		persist_variables[name] = value;
	}
}

void Context::set_variable(const std::string &name, const Value &value, bool persistent /*= true*/)
{
	set_variable(name, ValuePtr(value), persistent);
}

void Context::apply_variables(const Context &other)
{
	for (ValueMap::const_iterator it = other.variables.begin();it != other.variables.end();it++) {
		set_variable((*it).first, (*it).second);
	}
}

ValuePtr Context::lookup(const std::string &name, bool silent) const
{
	return lookup_variable(name, silent);
}

ValuePtr Context::lookup_variable(const std::string &name, bool silent) const
{
	if (!this->ctx_stack) {
		PRINT("ERROR: Context had null stack in lookup_variable()!!");
		return ValuePtr::undefined;
	}
	if (is_config_variable(name)) {
		for (int i = this->ctx_stack->size()-1; i >= 0; i--) {
			const ValueMap &confvars = ctx_stack->at(i)->config_variables;
			if (confvars.find(name) != confvars.end())
				return confvars.find(name)->second;
		}
		return ValuePtr::undefined;
	}
	const Context *pp = this;
	do {
		auto found = pp->variables.find(name);
		if (found != pp->variables.end())
			return found->second;
		pp = pp->parent;
	} while (pp);
	if (!silent)
		print_ignore_warning("variable", name.c_str());
	return ValuePtr::undefined;
}

bool Context::has_local_variable(const std::string &name) const
{
	if (is_config_variable(name))
		return config_variables.find(name) != config_variables.end();
	return variables.find(name) != variables.end();
}
 
ValuePtr Context::evaluate_function(const std::string &name, const EvalContext *evalctx) const
{
	const Context *pp = this;
	while (pp) {
		if (auto ff = pp->findLocalFunction(name))
			return ff->evaluate(this, evalctx);
		pp = pp->parent;
	}
	print_ignore_warning("function", name.c_str());
	return ValuePtr::undefined;
}

AbstractNode *Context::instantiate_module(const ModuleContext *evalctx) const
{
	const Context *pp = this;
	while (pp) {
		if (auto mm = pp->findLocalModule(evalctx->name()))
			return mm->instantiate(this, evalctx);
		pp = pp->parent;
	}
	print_ignore_warning("module", evalctx->name().c_str());
	return NULL;
}

/*!
	Returns the absolute path to the given filename, unless it's empty.
 */
std::string Context::getAbsolutePath(const std::string &filename) const
{
	if (!filename.empty() && !fs::path(filename).is_absolute()) {
		return fs::absolute(fs::path(this->document_path) / filename).string();
	}
	else {
		return filename;
	}
}

std::string Context::toString() const
{
	std::stringstream str;
	bool first = true;
	for (auto &arg : persist_variables) {
		if (!arg.second->isDefined())
			continue;
		if (!first)
			str << ", ";
		else
			first = false;
		str << arg.first << " = " << arg.second->toString();
	}
	return str.str();
}

#ifdef DEBUG
std::string Context::dump(const AbstractModule *mod, const ModuleInstantiation *inst)
{
	std::stringstream s;
	if (inst)
		s << boost::format("ModuleContext %p (%p) for %s inst (%p)") % this % this->parent % inst->name() % inst;
	else
		s << boost::format("Context: %p (%p)") % this % this->parent;
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

