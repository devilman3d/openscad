#include "localscope.h"
#include "modcontext.h"
#include "module.h"
#include "ModuleInstantiation.h"
#include "UserModule.h"
#include "expression.h"
#include "expressions.h"
#include "function.h"
#include "annotation.h"
#include "AST.h"
#include "FactoryModule.h"
#include "node.h"

NamedASTNode::NamedASTNode(const Assignment &ass)
	: name(ass.name)
	, node(ass.expr)
{
}

NamedASTNode::NamedASTNode(std::string name, UserStruct *s)
	: name(name)
	, node(s)
{
}

NamedASTNode::NamedASTNode(std::string name, UserFunction *f)
	: name(name)
	, node(f)
{
}

NamedASTNode::NamedASTNode(std::string name, UserModule *m)
	: name(name)
	, node(m)
{
}

NamedASTNode::NamedASTNode(std::string name, ModuleInstantiation *mi)
	: name(name)
	, node(mi)
{
}

NamedASTNode::NamedASTNode(std::string name, class Expression *e)
	: name(name)
	, node(e)
{
}

void LocalScope::addChild(ModuleInstantiation *modinst) 
{
	assert(modinst);
	this->children.push_back(modinst);
	this->orderedDefinitions.push_back(NamedASTNode(modinst->name(), modinst));
}

void LocalScope::addModule(class UserModule *module)
{
	assert(module);
	this->modules[module->name] = module;
	this->orderedDefinitions.push_back(NamedASTNode(module->name, module));
}

void LocalScope::addStruct(class UserStruct *module)
{
	assert(module);
	this->orderedDefinitions.push_back(NamedASTNode(module->name, module));
}

void LocalScope::addFunction(class UserFunction *func)
{
	assert(func);
	this->functions[func->name] = func;
	this->orderedDefinitions.push_back(NamedASTNode(func->name, func));
}

void LocalScope::addValue(const std::string &name, const ValuePtr &value)
{
	this->orderedDefinitions.push_back(NamedASTNode(name, new Literal(value)));
}

void LocalScope::addAssignment(const Assignment &ass)
{
	this->orderedDefinitions.push_back(NamedASTNode(ass));
}

void LocalScope::addResult(class Expression *astnode)
{
	this->orderedDefinitions.push_back(NamedASTNode("@result", astnode));
}

LocalScope LocalScope::operator+(const LocalScope &other) const
{
	Value::ScopeType sum = *this;
	for (auto &d : other.orderedDefinitions) {
		// retain the in-order info (and shared pointers)
		sum.orderedDefinitions.push_back(d);
		// override functions and modules
		if (auto f = dynamic_pointer_cast<UserFunction>(d.node)) {
			sum.functions[d.name] = f.get();
		}
		else if (auto m = dynamic_pointer_cast<UserModule>(d.node)) {
			sum.modules[d.name] = m.get();
		}
	}
	return sum;
}

bool LocalScope::operator==(const LocalScope &other) const
{
	if (orderedDefinitions.size() != other.orderedDefinitions.size())
		return false;
	for (int i = 0; i < orderedDefinitions.size(); ++i) {
		if (orderedDefinitions[i].name != other.orderedDefinitions[i].name)
			return false;
		// todo: this probably needs more attention
		if (orderedDefinitions[i].node != other.orderedDefinitions[i].node)
			return false;
	}
	return true;
}

void LocalScope::print(std::ostream &stream) const
{
	stream << dump("");
}

std::string LocalScope::dump(const std::string &indent) const
{
	std::stringstream dump;
	for (const auto &aos : this->orderedDefinitions) {
		if (auto f = dynamic_pointer_cast<UserFunction>(aos.node)) {
			dump << f->dump(indent, aos.name);
		}
		else if (auto m = dynamic_pointer_cast<UserModule>(aos.node)) {
			dump << m->dump(indent, aos.name);
		}
		else if (auto s = dynamic_pointer_cast<UserStruct>(aos.node)) {
			dump << s->dump(indent);
		}
		else if (auto e = dynamic_pointer_cast<Expression>(aos.node)) {
			/*
			if (ass.hasAnnotations()) {
				const Annotation *group = ass.annotation("Group");
				if (group != NULL) dump << group->dump();
				const Annotation *Description = ass.annotation("Description");
				if (Description != NULL) dump << Description->dump();
				const Annotation *parameter = ass.annotation("Parameter");
				if (parameter != NULL) dump << parameter->dump();
			}
			*/
			if (aos.name == "@result")
				dump << indent << "return " << *e << ";\n";
			else
				dump << indent << aos.name << " = " << *e << ";\n";
		}
		else if (auto mi = dynamic_pointer_cast<ModuleInstantiation>(aos.node)) {
			dump << mi->dump(indent);
		}
	}
	return dump.str();
}

/*!
	When instantiating a module which can take a scope as parameter (i.e. non-leaf nodes),
	use this method to apply the local scope definitions to the evaluation context.
	This will enable variables defined in local blocks.
	NB! for loops are special as the local block may depend on variables evaluated by the
	for loop parameters. The for loop code will handle this specially.
*/
void LocalScope::apply(Context &ctx) const
{
	for (const auto &aos : this->orderedDefinitions) {
		if (auto e = dynamic_pointer_cast<Expression>(aos.node)) {
			ctx.set_variable(aos.name, e->evaluate(&ctx));
		}
	}
}

NodeHandle simplify(const Context &c, const NodeHandle &node)
{
	if (auto g = dynamic_pointer_cast<GroupNode>(node)) {
		auto temp = NodeHandle(GroupNode::create(g->nodeFlags));
		for (auto &gc : g->getChildren())
			if (auto sgc = simplify(c, gc))
				temp->addChild(c, sgc);
		if (temp->getChildren().size() == 1)
			return temp->getChildren().front();
		return temp;
	}
	return node;
}

void LocalScope::evaluate(Context &ctx, NodeHandles &children) const
{
	apply(ctx);
	for (const auto &aos : this->orderedDefinitions) {
		if (auto mi = dynamic_pointer_cast<ModuleInstantiation>(aos.node)) {
			if (auto inst = NodeHandle(mi->evaluate(&ctx))) {
				auto simp = simplify(ctx, inst);
				children.push_back(simp);
			}
		}
	}
}
