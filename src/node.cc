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

#include "node.h"
#include "context.h"
#include "evalcontext.h"
#include "module.h"
#include "ModuleInstantiation.h"
#include "progress.h"
#include "stl-utils.h"
#include "value.h"
#include "Geometry.h"

#include "cgal.h"
#include "cgalutils.h"
#include "CGAL_Nef_polyhedron.h"

#include <iostream>
#include <algorithm>

size_t AbstractNode::idx_counter(1);

AbstractNode::AbstractNode()
	: /*parent(nullptr)
	, */nodeFlags(NodeFlags::None)
	, idx(idx_counter++)
{
}

AbstractNode::~AbstractNode()
{
}

size_t AbstractNode::indexOfChild(const AbstractNode *child) const
{
	for (int i = 0; i < children.size(); ++i)
		if (children[i].get() == child)
			return i;
	return -1;
}

std::string AbstractNode::toString() const
{
	return this->name() + "()";
}

std::ostream &operator<<(std::ostream &stream, const AbstractNode &node)
{
	stream << node.toString();
	return stream;
}

// Do we have an explicit root node (! modifier)?
const AbstractNode *find_root_tag(const AbstractNode *n)
{
  for(auto &v : n->getChildren()) {
    if (v->isRoot()) 
		return v.get();
    if (auto vroot = find_root_tag(v.get())) 
		return vroot;
  }
  return nullptr;
}