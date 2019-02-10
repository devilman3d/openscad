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

#include "import.h"
#include "importnode.h"

#include "module.h"
#include "ModuleInstantiation.h"
#include "polyset.h"
#ifdef ENABLE_CGAL
#include "CGAL_Nef_polyhedron.h"
#endif
#include "Polygon2d.h"
#include "evalcontext.h"
#include "builtin.h"
#include "dxfdata.h"
#include "printutils.h"
#include "fileutils.h"
#include "feature.h"
#include "FactoryNode.h"

#include <sys/types.h>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <boost/assign/std/vector.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

#include <boost/detail/endian.hpp>
#include <cstdint>

extern PolySet * import_amf(std::string);

class ImportNode : public FactoryNode
{
public:
	VISITABLE();

	ImportNode(import_type_e type = TYPE_UNKNOWN)
		: FactoryNode("file", "layer", "convexity", "origin", "scale")
		, type(type)
		, fn(0), fs(0), fa(0)
		, origin_x(0), origin_y(0), scale(1)
		, width(0), height(0)
		, center(false)
	{
	}

	import_type_e type;
	Filename filename;
	std::string layername;
	double fn, fs, fa;
	double origin_x, origin_y, scale;
	double width, height;
	bool center;

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		ValuePtr v = c.lookup_variable("file");
		if (v->isUndefined()) {
			v = c.lookup_variable("filename");
			if (!v->isUndefined()) {
				printDeprecation("filename= is deprecated. Please use file=");
			}
		}
		std::string filename = lookup_file(v->isUndefined() ? "" : v->toString(), evalctx.location().path(), c.documentPath());
		import_type_e actualtype = this->type;
		if (actualtype == TYPE_UNKNOWN) {
			std::string extraw = fs::path(filename).extension().generic_string();
			std::string ext = boost::algorithm::to_lower_copy(extraw);
			if (ext == ".stl") actualtype = TYPE_STL;
			else if (ext == ".off") actualtype = TYPE_OFF;
			else if (ext == ".dxf") actualtype = TYPE_DXF;
			else if (ext == ".nef3") actualtype = TYPE_NEF3;
			else if (ext == ".obj") actualtype = TYPE_OBJ;
			else if (Feature::ExperimentalAmfImport.is_enabled() && ext == ".amf") actualtype = TYPE_AMF;
			else if (Feature::ExperimentalSvgImport.is_enabled() && ext == ".svg") actualtype = TYPE_SVG;
		}

		this->fn = c.lookup_variable("$fn")->toDouble();
		this->fs = c.lookup_variable("$fs")->toDouble();
		this->fa = c.lookup_variable("$fa")->toDouble();

		this->filename = filename;
		Value layerval = *c.lookup_variable("layer", true);
		if (layerval.isUndefined()) {
			layerval = *c.lookup_variable("layername");
			if (!layerval.isUndefined()) {
				printDeprecation("layername= is deprecated. Please use layer=");
			}
		}
		this->layername = layerval.isUndefined() ? "" : layerval.toString();

		ValuePtr origin = c.lookup_variable("origin", true);
		this->origin_x = this->origin_y = 0;
		origin->getVec2(this->origin_x, this->origin_y);

		this->scale = c.lookup_variable("scale", true)->toDouble();

		if (this->scale <= 0) this->scale = 1;

		ValuePtr width = c.lookup_variable("width", true);
		ValuePtr height = c.lookup_variable("height", true);
		this->width = (width->type() == Value::NUMBER) ? width->toDouble() : -1;
		this->height = (height->type() == Value::NUMBER) ? height->toDouble() : -1;

		this->center = false;
		ValuePtr center = c.lookup_variable("center", true);
		if (center->type() == Value::BOOL)
			this->center = center->toBool();

		// add timestamp to the context
		fs::path path((std::string)this->filename);
		time_t ts = fs::exists(path) ? fs::last_write_time(path) : 0;
		c.set_variable("timestamp", ValuePtr((double)ts));
	}

	/*!
		Will return an empty geometry if the import failed, but not NULL
	*/
	ResultObject processChildren(const NodeGeometries &children) const override
	{
		Geometry *g = NULL;

		switch (this->type) {
		case TYPE_STL: {
			auto temp = import_stl(this->filename);
			if (center)
				temp->transform(Eigen::Affine3d(Eigen::Translation3d(-temp->getBoundingBox().center())));
			g = temp;
			break;
		}
		case TYPE_AMF: {
			auto temp = import_amf(this->filename);
			if (center)
				temp->transform(Eigen::Affine3d(Eigen::Translation3d(-temp->getBoundingBox().center())));
			g = temp;
			break;
		}
		case TYPE_OFF: {
			auto temp = import_off(this->filename);
			if (center)
				temp->transform(Eigen::Affine3d(Eigen::Translation3d(-temp->getBoundingBox().center())));
			g = temp;
			break;
		}
		case TYPE_SVG: {
			auto temp = import_svg(this->filename);
			if (center) {
				auto c3 = temp->getBoundingBox().center();
				temp->transform(Eigen::Affine2d(Eigen::Translation2d(-Vector2d(c3[0], c3[1]))));
			}
			g = temp;
			break;
		}
		case TYPE_DXF: {
			DxfData dd(this->fn, this->fs, this->fa, this->filename, this->layername, this->origin_x, this->origin_y, this->scale);
			auto temp = dd.toPolygon2d();
			if (center) {
				auto c3 = temp->getBoundingBox().center();
				temp->transform(Eigen::Affine2d(Eigen::Translation2d(-Vector2d(c3[0], c3[1]))));
			}
			g = temp;
			break;
		}
		case TYPE_OBJ: {
			auto objs = import_obj(this->filename);
			BoundingBox bb;
			for (auto *ps : objs)
				bb.extend(ps->getBoundingBox());
			NodeGeometries geoms;
			for (auto *ps : objs) {
				ps->setConvexity(this->convexity);
				if (center)
					ps->transform(Eigen::Affine3d(Eigen::Translation3d(-bb.center())));
				geoms.push_back(NodeGeometry(this, std::shared_ptr<PolySet>(ps)));
			}
			g = new GeometryGroup(geoms);
			break;
		}
#ifdef ENABLE_CGAL
		case TYPE_NEF3: {
			auto temp = import_nef3(this->filename);
			if (center)
				temp->transform(Eigen::Affine3d(Eigen::Translation3d(-temp->getBoundingBox().center())));
			g = temp;
			break;
		}
#endif
		default:
			PRINTB("ERROR: Unsupported file format while trying to import file '%s'", this->filename);
			g = new PolySet(0);
		}

		if (g) {
			g->setConvexity(this->convexity);
			PRINT("Import successful:");
			std::stringstream stream;
			const auto &bb = g->getBoundingBox();
			stream << "    Bounding box: [" << bb.min().transpose() << "]-[" << bb.max().transpose() << "]";
			PRINT(stream.str());
			stream.str("");
			stream << "    Center: [" << bb.center().transpose() << "]";
			PRINT(stream.str());
		}
		return ResultObject(g);
	}
};

FactoryModule<ImportNode> ImportNodeFactory("import");
