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

#include "offsetnode.h"

#include "module.h"
#include "ModuleInstantiation.h"
#include "evalcontext.h"
#include "printutils.h"
#include "fileutils.h"
#include "builtin.h"
#include "calc.h"
#include "polyset.h"
#include "value.h"
#include "clipper-utils.h"
#include "FactoryNode.h"
#include "maybe_const.h"

#include <sstream>
#include <boost/assign/std/vector.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class OffsetNode : public FactoryNode
{
public:
	OffsetNode() : FactoryNode(
		"r", "delta", "chamfer",
		"$fn", "$fs", "$fa")
		, fn(0), fs(0), fa(0), delta(1), miter_limit(1000000.0), join_type(ClipperLib::jtRound) { }

	bool chamfer;
	double fn, fs, fa, delta;
	double miter_limit; // currently fixed high value to disable chamfers with jtMiter
	ClipperLib::JoinType join_type;

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		this->fn = c.lookup_variable("$fn")->toDouble();
		this->fs = c.lookup_variable("$fs")->toDouble();
		this->fa = c.lookup_variable("$fa")->toDouble();

		// default with no argument at all is (r = 1, chamfer = false)
		// radius takes precedence if both r and delta are given.
		this->delta = 1;
		this->chamfer = false;
		this->join_type = ClipperLib::jtRound;
		const ValuePtr r = c.lookup_variable("r", true);
		const ValuePtr delta = c.lookup_variable("delta", true);
		const ValuePtr chamfer = c.lookup_variable("chamfer", true);

		if (r->isDefinedAs(Value::NUMBER)) {
			r->getDouble(this->delta);
		}
		else if (delta->isDefinedAs(Value::NUMBER)) {
			delta->getDouble(this->delta);

			this->join_type = ClipperLib::jtMiter;
			if (chamfer->isDefinedAs(Value::BOOL) && chamfer->toBool()) {
				this->chamfer = true;
				this->join_type = ClipperLib::jtSquare;
			}
		}
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		Polygon2ds dim2;
		GeomUtils::collect(children, dim2);
		ClipperUtils _union;
		if (auto geometry = maybe_const<Polygon2d>(_union.apply(dim2, ClipperLib::ClipType::ctUnion))) {
			// ClipperLib documentation: The formula for the number of steps in a full
			// circular arc is ... Pi / acos(1 - arc_tolerance / abs(delta))
			double n = Calc::get_fragments_from_r(std::abs(this->delta), this->fn, this->fs, this->fa);
			double arc_tolerance = std::abs(this->delta) * (1 - cos(M_PI / n));
			ClipperUtils utils;
			auto result = maybe_const<Geometry>(utils.applyOffset(*geometry, this->delta, this->join_type, this->miter_limit, arc_tolerance));
			assert(result);
			return ResultObject(result);
		}
		return ResultObject(new EmptyGeometry());
	}
};

FactoryModule<OffsetNode> OffsetNodeFactory("offset");
