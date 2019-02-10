#pragma once

#include <string>
#include <unordered_map>
#include "module.h"
#include "localscope.h"

/*
	A "static" scope to provide access to builtin modules,
	functions and variables.
*/
class Builtins : public LocalScope
{
public:
	// add a [C++] module to the global scope
	static void init(const char *name, class AbstractModule *module);
	// add a [C++] function to the global scope
	static void init(const char *name, class AbstractFunction *function);
	// add a [C++] variable to the global scope
	static void init(const char *name, const class ValuePtr &value);

	// determines if the named module or function will be removed in a future release
	static std::string isDeprecated(const std::string &name);

	// gets the static Builtins scope
	static const LocalScope &getGlobalScope() { return *instance(); }

	// gets a space-separated string of keywords for the lexer
	//	index = 1: function names
	//	index = 2: module names
	static std::string getLexerKeywords(int index);

	// deletes the static instance
	static void release();
private:
	// private constructor
	Builtins() { }

	// get/release the static instance
	static Builtins *instance(bool erase = false);

	// separated initialize method called when the static instance is created
	void initialize();

	std::unordered_map<std::string, std::string> deprecations;
};
