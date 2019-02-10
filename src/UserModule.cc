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

#include "UserModule.h"
#include "ModuleInstantiation.h"
#include "node.h"
#include "evalcontext.h"
#include "exceptions.h"
#include "stackcheck.h"
#include "modcontext.h"
#include "expression.h"

#include <sstream>

// ctx = this module's lexical container: parent scope
// evalctx = eval context: parent = original caller, contains inst's args and scope; only ever used if children() is called later
AbstractNode *UserModule::instantiate(const Context *ctx, const ModuleContext *evalctx) const
{
	if (StackCheck::inst()->check()) {
		throw RecursionException::create("module", evalctx->name());
		return NULL;
	}
    
	UserContext uc(ctx, this, evalctx);
	uc.setName("UserModule", evalctx->name());

	AbstractNode *node = GroupNode::create(evalctx->flags());
	this->scope.evaluate(uc, node->getChildren());

	return node;
}

std::string UserModule::dump(const std::string &indent, const std::string &name) const
{
	std::stringstream dump;
	std::string tab;
	if (!name.empty()) {
		dump << indent << "module " << name << "(";
		for (size_t i=0; i < this->definition_arguments.size(); i++) {
			const Assignment &arg = this->definition_arguments[i];
			if (i > 0) dump << ", ";
			dump << arg.name;
			if (arg.expr) dump << " = " << *arg.expr;
		}
		dump << ") {\n";
		tab = "\t";
	}
	dump << scope.dump(indent + tab);
	if (!name.empty()) {
		dump << indent << "}\n";
	}
	return dump.str();
}
