#pragma once

#include "AST.h"

#include <string>

class Expression : public ASTNode
{
public:
	Expression(const Location &loc) : ASTNode(loc) {}
	virtual ~Expression() {}
    virtual bool isLiteral() const;
	virtual class ValuePtr evaluate(const class Context *context) const = 0;
	virtual void print(std::ostream &stream) const = 0;
};

std::ostream &operator<<(std::ostream &stream, const Expression &expr);
