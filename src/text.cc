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

#include "calc.h"
#include "module.h"
#include "evalcontext.h"
#include "printutils.h"
#include "builtin.h"

#include "textnode.h"
#include "FreetypeRenderer.h"
#include "FactoryNode.h"
#include "Polygon2d.h"
#include "clipper-utils.h"

#include <boost/assign/std/vector.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

class TextNode : public FactoryNode
{
public:
	TextNode() : FactoryNode(
		"text", "size", "font",
		"spacing", "direction", "language",
		"script", "halign", "valign",
		"$fn", "$fs", "$fa") { }

	FreetypeRenderer::Params params;

	double lookup_double_variable_with_default(const Context &c, const std::string &name, double dft)
	{
		ValuePtr v = c.lookup_variable(name, true);
		if (v->isDefinedAs(Value::NUMBER))
			v->getFiniteDouble(dft);
		return dft;
	}

	std::string lookup_string_variable_with_default(const Context &c, const std::string &name, const std::string &dft)
	{
		ValuePtr v = c.lookup_variable(name, true);
		if (v->isDefinedAs(Value::STRING))
			return v->toString();
		return dft;
	}

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		double fn = c.lookup_variable("$fn")->toDouble();
		double fa = c.lookup_variable("$fa")->toDouble();
		double fs = c.lookup_variable("$fs")->toDouble();

		this->params.set_fn(fn);
		this->params.set_fa(fa);
		this->params.set_fs(fs);

		double size = lookup_double_variable_with_default(c, "size", 10.0);
		int segments = Calc::get_fragments_from_r(size, fn, fs, fa);
		// The curved segments of most fonts are relatively short, so
		// by using a fraction of the number of full circle segments
		// the resolution will be better matching the detail level of
		// other objects.
		int text_segments = std::max(((int)floor(segments / 8)) + 1, 2);

		this->params.set_size(size);
		this->params.set_segments(text_segments);
		this->params.set_text(lookup_string_variable_with_default(c, "text", ""));
		this->params.set_spacing(lookup_double_variable_with_default(c, "spacing", 1.0));
		this->params.set_font(lookup_string_variable_with_default(c, "font", ""));
		this->params.set_direction(lookup_string_variable_with_default(c, "direction", ""));
		this->params.set_language(lookup_string_variable_with_default(c, "language", "en"));
		this->params.set_script(lookup_string_variable_with_default(c, "script", ""));
		this->params.set_halign(lookup_string_variable_with_default(c, "halign", "left"));
		this->params.set_valign(lookup_string_variable_with_default(c, "valign", "baseline"));

		FreetypeRenderer renderer;
		renderer.detect_properties(this->params);
	}

	std::vector<const Geometry *> createGeometryList() const
	{
		FreetypeRenderer renderer;
		return renderer.render(this->params);
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		std::vector<const Geometry *> geometrylist = createGeometryList();
		std::vector<shared_ptr<const Polygon2d>> sharedlist;
		std::vector<const Polygon2d *> polygonlist;
		for (const auto &geometry : geometrylist) {
			const Polygon2d *polygon = dynamic_cast<const Polygon2d*>(geometry);
			assert(polygon);
			polygonlist.push_back(polygon);
			sharedlist.push_back(shared_ptr<const Polygon2d>(polygon));
		}
		ClipperUtils utils;
		return ResultObject(utils.apply(polygonlist, ClipperLib::ctUnion));
	}
};

FactoryModule<TextNode> TextNodeFactory("text");
