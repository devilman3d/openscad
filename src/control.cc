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

#include "module.h"
#include "ModuleInstantiation.h"
#include "node.h"
#include "FactoryNode.h"
#include "Geometry.h"
#include "evalcontext.h"
#include "modcontext.h"
#include "expression.h"
#include "expressions.h"
#include "builtin.h"
#include "printutils.h"
#include <cstdint>
#include <sstream>

class ControlModule : public AbstractModule
{
public: // types
	enum Type {
		//CHILD,
		CHILDREN,
		ECHO,
		ASSERT,
		//ASSIGN,
		FOR,
		//LET,
		INT_FOR,
		IF
    };
public: // methods
	ControlModule(Type type) : type(type) { }

	ControlModule(Type type, const Feature& feature) : AbstractModule(feature), type(type) { }

	virtual AbstractNode *instantiate(const Context *ctx, const ModuleContext *evalctx) const;

	static void for_eval(AbstractNode &node, size_t l, 
						 const Context *ctx, const ModuleContext *evalctx);

	static const ModuleContext* getLastModuleCtx(const ModuleContext *evalctx);
	
	static AbstractNode* getChild(const ValuePtr &value, const ModuleContext* modulectx);

private: // data
	Type type;

}; // class ControlModule

void ControlModule::for_eval(AbstractNode &node, size_t l, 
							const Context *ctx, const ModuleContext *evalctx)
{
	if (evalctx->numArgs() > l) {
		const std::string &it_name = evalctx->getArgName(l);
		ValuePtr it_values = evalctx->getArgValue(l, ctx);
		Context c(ctx);
		c.setName("for", it_name + " = " + it_values->toString());
		if (it_values->type() == Value::RANGE) {
			RangeType range = it_values->toRange();
			uint32_t steps = range.numValues();
			if (steps >= 10000) {
				PRINTB("WARNING: Bad range parameter in for statement: too many elements (%lu).", steps);
			} else {
				for (RangeType::iterator it = range.begin();it != range.end();it++) {
					c.set_variable(it_name, ValuePtr(*it));
					for_eval(node, l+1, &c, evalctx);
				}
			}
		}
		else if (it_values->type() == Value::VECTOR) {
			for (size_t i = 0; i < it_values->toVector().size(); i++) {
				c.set_variable(it_name, it_values->toVector()[i]);
				for_eval(node, l+1, &c, evalctx);
			}
		}
		else if (it_values->type() != Value::UNDEFINED) {
			c.set_variable(it_name, it_values);
			for_eval(node, l+1, &c, evalctx);
		}
	} else if (l > 0) {
		// At this point, the for loop variables have been set and we can initialize
		// the local scope (as they may depend on the for loop variables
		Context c(ctx);
		c.setName("for", "evaluate");
		evalctx->evaluate(c, node.getChildren());
	}
}

const ModuleContext* ControlModule::getLastModuleCtx(const ModuleContext *evalctx)
{
	// Find the last custom module invocation, which will contain
	// an eval context with the children of the module invokation
	const Context *pp = evalctx->getParent();
	while (pp) {
		if (auto uc = dynamic_cast<const UserContext*>(pp))
			return uc->getModuleContext();
		pp = pp->getParent();
	}
	return nullptr;
}

// static
AbstractNode* ControlModule::getChild(const ValuePtr &value, const ModuleContext* modulectx)
{
	if (value->type()!=Value::NUMBER) {
		// Invalid parameter
		// (e.g. first child of difference is invalid)
		PRINTB("WARNING: Bad parameter type (%s) for children, only accept: empty, number, vector, range.", value->toString());
		return NULL;
	}
	double v;
	if (!value->getDouble(v)) {
		PRINTB("WARNING: Bad parameter type (%s) for children, only accept: empty, number, vector, range.", value->toString());
		return NULL;
	}
		
	int n = trunc(v);
	if (n < 0) {
		PRINTB("WARNING: Negative children index (%d) not allowed", n);
		return NULL; // Disallow negative child indices
	}
	if (n>=(int)modulectx->numChildren()) {
		// How to deal with negative objects in this case?
		// (e.g. first child of difference is invalid)
		PRINTB("WARNING: Children index (%d) out of bounds (%d children)"
			, n % modulectx->numChildren());
		return NULL;
	}
	// OK
	return modulectx->getChild(n)->evaluate(modulectx);
}

AbstractNode *ControlModule::instantiate(const Context* ctx, const ModuleContext *evalctx) const
{
	AbstractNode *node = NULL;

	switch (this->type) {
	//case CHILD:	{
	//	printDeprecation("child() will be removed in future releases. Use children() instead.");
	//	int n = 0;
	//	if (evalctx->numArgs() > 0) {
	//		double v;
	//		if (evalctx->getArgValue(0)->getDouble(v)) {
	//			n = trunc(v);
	//			if (n < 0) {
	//				PRINTB("WARNING: Negative child index (%d) not allowed", n);
	//				return NULL; // Disallow negative child indices
	//			}
	//		}
	//	}
	//
	//	// Find the last custom module invocation, which will contain
	//	// an eval context with the children of the module invokation
	//	auto *modulectx = getLastModuleCtx(evalctx);
	//	if (modulectx==NULL) {
	//		return NULL;
	//	}
	//	// This will trigger if trying to invoke child from the root of any file
    //    if (n < (int)modulectx->numChildren()) {
	//		node = modulectx->getChild(n)->evaluate(modulectx);
	//	}
	//	else {
	//		// How to deal with negative objects in this case?
    //        // (e.g. first child of difference is invalid)
	//		PRINTB("WARNING: Child index (%d) out of bounds (%d children)", 
	//			   n % modulectx->numChildren());
	//	}
	//	return node;
	//}
	//	break;

	case CHILDREN: {
		auto *modulectx = getLastModuleCtx(evalctx);
		if (modulectx==NULL) {
			return NULL;
		}
		// This will trigger if trying to invoke child from the root of any file
		// assert(filectx->evalctx);
		if (evalctx->numArgs()<=0) {
			// no parameters => all children
			AbstractNode* node = GroupNode::create(evalctx->flags());
			for (int n = 0; n < (int)modulectx->numChildren(); ++n) {
				AbstractNode* childnode = modulectx->getChild(n)->evaluate(modulectx);
				if (childnode==NULL) continue; // error
				node->addChild(*ctx, NodeHandle(childnode));
			}
			return node;
		}
		else if (evalctx->numArgs()>0) {
			// one (or more ignored) parameter
			ValuePtr value = evalctx->getArgValue(0);
			if (value->type() == Value::NUMBER) {
				return getChild(value, modulectx);
			}
			else if (value->type() == Value::VECTOR) {
				AbstractNode* node = GroupNode::create(evalctx->flags());
				const Value::VectorType& vect = value->toVector();
				for(const auto &vectvalue : vect) {
					AbstractNode* childnode = getChild(vectvalue,modulectx);
					if (childnode==NULL) continue; // error
					node->addChild(*ctx, NodeHandle(childnode));
				}
				return node;
			}
			else if (value->type() == Value::RANGE) {
				RangeType range = value->toRange();
				uint32_t steps = range.numValues();
				if (steps >= 10000) {
					PRINTB("WARNING: Bad range parameter for children: too many elements (%lu).", steps);
					return NULL;
				}
				AbstractNode* node = GroupNode::create(evalctx->flags());
				for (RangeType::iterator it = range.begin();it != range.end();it++) {
					AbstractNode* childnode = getChild(ValuePtr(*it),modulectx); // with error cases
					if (childnode==NULL) continue; // error
					node->addChild(*ctx, NodeHandle(childnode));
				}
				return node;
			}
			else {
				// Invalid parameter
				// (e.g. first child of difference is invalid)
				PRINTB("WARNING: Bad parameter type (%s) for children, only accept: empty, number, vector, range.", value->toString());
				return NULL;
			}
		}
		return NULL;
	}
		break;

	case ECHO: {
		node = GroupNode::create(evalctx->flags());
		std::stringstream msg;
		msg << "ECHO: " << *evalctx;
		PRINTB("%s", msg.str());
		Context c(evalctx);
		c.setName("ECHO", "evaluate");
		evalctx->evaluate(c, node->getChildren());
	}
		break;

	case ASSERT: {
		node = GroupNode::create(evalctx->flags());
		Context c(evalctx);
		c.setName("ASSERT", "evaluate");
		evaluate_assert(c, evalctx, evalctx->location());
		evalctx->evaluate(c, node->getChildren());
	}
		break;

	case FOR:
		node = GroupNode::create(evalctx->flags());
		for_eval(*node, 0, evalctx, evalctx);
		break;

	case INT_FOR:
		node = AbstractIntersectionNode::create(evalctx->flags());
		for_eval(*node, 0, evalctx, evalctx);
		break;

	case IF: {
		node = GroupNode::create(evalctx->flags());
		auto *inst = evalctx->getModuleInstantiation();
		const IfElseModuleInstantiation *ifelse = dynamic_cast<const IfElseModuleInstantiation*>(inst);
		auto &scope = (evalctx->numArgs() > 0 && evalctx->getArgValue(0)->toBool()) ? inst->scope : ifelse->else_scope;
		Context c(evalctx);
		c.setName("IF", "evaluate");
		scope.evaluate(c, node->getChildren());
	}
		break;
	}
	return node;
}

void register_builtin_control()
{
	//Builtins::init("child", new ControlModule(ControlModule::CHILD));
	Builtins::init("children", new ControlModule(ControlModule::CHILDREN));
	Builtins::init("echo", new ControlModule(ControlModule::ECHO));
	Builtins::init("assert", new ControlModule(ControlModule::ASSERT, Feature::ExperimentalAssertExpression));
	//Builtins::init("assign", new ControlModule(ControlModule::ASSIGN));
	Builtins::init("for", new ControlModule(ControlModule::FOR));
	//Builtins::init("let", new ControlModule(ControlModule::LET));
	Builtins::init("intersection_for", new ControlModule(ControlModule::INT_FOR));
	Builtins::init("if", new ControlModule(ControlModule::IF));
}
