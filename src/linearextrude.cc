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

#include "linearextrudenode.h"

#include "node.h"
#include "FactoryNode.h"
#include "module.h"
#include "ModuleInstantiation.h"
#include "evalcontext.h"
#include "printutils.h"
#include "fileutils.h"
#include "builtin.h"
#include "calc.h"
#include "polyset.h"
#include "Geometry.h"
#include "cgalutils.h"
#include "clipper-utils.h"
#include "dxfdata.h"
#include "PathHelpers.h"
#include "progress.h"

#include <cmath>
#include <sstream>

class LinearExtrudeNode : public FactoryNode
{
public:
	LinearExtrudeNode() : FactoryNode(
		"height", "slices", "center", "twist",
		"rtwist", "irtwist",
		"t0", "t1",
		"scale",
		"vscale", "rscale", "irscale",
		"origin", 
		"path",
		"morph",
		"$fn", "$fs", "$fa")
		, slices(0)
		, fn(0), fs(0), fa(0)
		, height(100)
		, t0(0), t1(1)
		, twist(0), rtwist(0), irtwist(0)
		, origin_x(0), origin_y(0)
		, scale_x(1), scale_y(1), vscale_x(1), vscale_y(1)
		, rscale_x(1), rscale_y(1), irscale_x(1), irscale_y(1)
		, center(false), morph(false)
		, path(Value::undefined)
	{
	}

	int slices;
	double fn, fs, fa;
	double height;
	double t0, t1;
	double twist, rtwist, irtwist;
	double origin_x, origin_y;
	double scale_x, scale_y, vscale_x, vscale_y;
	double rscale_x, rscale_y, irscale_x, irscale_y;
	bool center, morph;
	ValuePtr path;

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		this->fn = c.lookup("$fn")->toDouble();
		this->fs = c.lookup("$fs")->toDouble();
		this->fa = c.lookup("$fa")->toDouble();

		ValuePtr height = c.lookup("height");
		ValuePtr origin = c.lookup("origin");
		ValuePtr t0 = c.lookup("t0");
		ValuePtr t1 = c.lookup("t1");
		ValuePtr scale = c.lookup("scale");
		ValuePtr vscale = c.lookup("vscale");
		ValuePtr rscale = c.lookup("rscale");
		ValuePtr irscale = c.lookup("irscale");
		ValuePtr center = c.lookup("center");
		ValuePtr twist = c.lookup("twist");
		ValuePtr rtwist = c.lookup("rtwist");
		ValuePtr irtwist = c.lookup("irtwist");
		ValuePtr slices = c.lookup("slices");
		ValuePtr path = c.lookup("path");
		ValuePtr morph = c.lookup("morph");

		// if height not given, and first argument is a number,
		// then assume it should be the height.
		if (c.lookup_variable("height")->isUndefined() &&
			evalctx.numArgs() > 0 &&
			evalctx.getArgName(0) == "") {
			ValuePtr val = evalctx.getArgValue(0);
			if (val->type() == Value::NUMBER) height = val;
		}

		this->height = 100;
		height->getFiniteDouble(this->height);
		if (this->height <= 0) 
			this->height = 0;

		t0->getFiniteDouble(this->t0);
		t1->getFiniteDouble(this->t1);
		this->t0 = std::max(0.0, std::min(1.0, this->t0));
		this->t1 = std::max(0.0, std::min(1.0, this->t1));
		//if (this->t0 > this->t1)
		//	std::swap(this->t0, this->t1);

		origin->getVec2(this->origin_x, this->origin_y, true);

		this->scale_x = this->scale_y = 1;
		scale->getFiniteDouble(this->scale_x);
		scale->getFiniteDouble(this->scale_y);
		scale->getVec2(this->scale_x, this->scale_y, true);

		if (this->scale_x < 0) this->scale_x = 0;
		if (this->scale_y < 0) this->scale_y = 0;

		this->vscale_x = this->vscale_y = 1;
		vscale->getFiniteDouble(this->vscale_x);
		vscale->getFiniteDouble(this->vscale_y);
		vscale->getVec2(this->vscale_x, this->vscale_y, true);

		if (this->vscale_x < 0) this->vscale_x = 0;
		if (this->vscale_y < 0) this->vscale_y = 0;

		this->rscale_x = this->rscale_y = 1;
		rscale->getFiniteDouble(this->rscale_x);
		rscale->getFiniteDouble(this->rscale_y);
		rscale->getVec2(this->rscale_x, this->rscale_y, true);

		if (this->rscale_x < 0) this->rscale_x = 0;
		if (this->rscale_y < 0) this->rscale_y = 0;

		this->irscale_x = this->irscale_y = 1;
		irscale->getFiniteDouble(this->irscale_x);
		irscale->getFiniteDouble(this->irscale_y);
		irscale->getVec2(this->irscale_x, this->irscale_y, true);

		if (this->irscale_x < 0) this->irscale_x = 0;
		if (this->irscale_y < 0) this->irscale_y = 0;

		if (center->type() == Value::BOOL)
			this->center = center->toBool();

		this->twist = 0.0;
		twist->getFiniteDouble(this->twist);
		this->rtwist = 0.0;
		rtwist->getFiniteDouble(this->rtwist);
		this->irtwist = 0.0;
		irtwist->getFiniteDouble(this->irtwist);

		double slicesVal = 0;
		slices->getFiniteDouble(slicesVal);
		this->slices = (int)slicesVal;

		if (this->slices == 0) {
			auto tw = this->rtwist + this->irtwist + this->twist;
			if (tw) {
				this->slices = (int)fmax(2, fabs(Calc::get_fragments_from_r(this->height, this->fn, this->fs, this->fa) * tw / 360));
			}
			else if (this->rscale_x != 1.0 || this->rscale_y != 1.0 || this->irscale_x != 1.0 || this->irscale_y != 1.0) {
				this->slices = (int)fmax(2, fabs(Calc::get_fragments_from_r(this->height, this->fn, this->fs, this->fa)));
			}
		}
		this->slices = std::max(this->slices, 1);

		this->path = path;

		this->morph = morph->toBool();
	}

	void collectPolygons(const NodeGeometries &children, std::vector<const Polygon2d*> &polygons) const
	{
		for (auto child : children) {
			if (auto p2d = dynamic_pointer_cast<const Polygon2d>(child.second))
				polygons.push_back(new Polygon2d(*p2d));
			else if (auto g = dynamic_pointer_cast<const GeometryGroup>(child.second)) {
				collectPolygons(g->getChildren(), polygons);
			}
		}
	}

	/*!
		input: List of 2D objects
		output: 3D PolySet
		operation:
		  o Union all children
		  o Perform extrude
	 */
	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		Polygon2dHandles polygons;
		GeomUtils::collect(children, polygons);
		if (morph)
		{
			if (polygons.size() > 2)
				PRINT("WARNING: Linear Extrude with morph only supports two polygons. The first and last will be used.");
		}
		else if (polygons.size() > 1) {
			ClipperUtils utils;
			auto geometry = Polygon2dHandle(utils.apply(polygons, ClipperLib::ClipType::ctUnion));
			polygons.clear();
			polygons.push_back(geometry);
		}
		if (!polygons.empty()) {
			auto extruded = extrudePolygon(polygons);
			assert(extruded);
			return ResultObject(extruded);
		}
		return ResultObject(new EmptyGeometry());
	}

	/*!
		Input to extrude should be sanitized. This means non-intersecting, correct winding order
		etc., the input coming from a library like Clipper.
	*/
	Geometry *extrudePolygon(const Polygon2dHandles &polys) const
	{
		if (height <= 0) 
			return new PolySet(3);

		bool cvx = true;
		for (auto poly : polys)
		{
			if (!poly->is_convex())
			{
				cvx = false;
				break;
			}
		}
		if (rscale_x != 1 || rscale_y != 1 || irscale_x != 1 || irscale_y != 1)
			cvx = false;
		PolySet *ps = new PolySet(3, !cvx ? boost::tribool(false) : (twist == 0 && rtwist == 0 && irtwist == 0) ? boost::tribool(true) : unknown);
		ps->setConvexity(convexity);

		Polygon2d polyBot(*polys.front());
		Polygon2d polyTop(*polys.back());

		const SliceSettings firstSlice(0.0, *this);
		const SliceSettings lastSlice(1.0, *this);

		// bottom
		if (true)
		{
			Polygon2d bot_poly(polyBot);

			bot_poly.transform(firstSlice.vertTransform);
			PolySet *ps_bot = bot_poly.tessellate();

			Vector3d originBot(0, 0, firstSlice.z);
			ps_bot->translate(originBot);

			// Flip vertex ordering for bottom polygon
			for (Polygon &p : ps_bot->getPolygons()) {
				std::reverse(p.begin(), p.end());
			}

			ps->append(*ps_bot);
			delete ps_bot;
		}

		// top
		if (true)
		{
			if (!lastSlice.scale0) {
				Polygon2d top_poly(polyTop);

				top_poly.transform(lastSlice.vertTransform); // top
				PolySet *ps_top = top_poly.tessellate();

				Vector3d originTop(0, 0, lastSlice.z);
				ps_top->translate(originTop);

				ps->append(*ps_top);
				delete ps_top;
			}
		}

		LocalProgress progress("Extruding", slices);
		if (morph)
		{
			PolyMorpher morpher(&polyBot, &polyTop);
			SliceSettings bot(firstSlice);
			Polygon2d pBot;
			morpher.generatePolygon(bot.t, pBot);
			for (auto j = 0; j < slices; j++)
			{
				double t1 = (double)(j + 1) / slices;
				SliceSettings top(t1, *this);
				Polygon2d pTop;
				morpher.generatePolygon(top.t, pTop);
				SliceSettings::add_slice(ps, pBot, pTop, bot, top);
				pBot = pTop;
				bot = top;
				progress.tick();
			}
		}
		else
		{
			SliceSettings bot(firstSlice);
			for (auto j = 0; j < slices; j++)
			{
				double t1 = (double)(j + 1) / slices;
				SliceSettings top(t1, *this);
				SliceSettings::add_slice(ps, polyBot, bot, top);
				bot = top;
				progress.tick();
			}
		}

		//PRINT("Finished extrusion");
		return ps;
	}
};

SliceSettings::SliceSettings(double time, const LinearExtrudeNode &node)
	: fn(node.fn)
	, fs(node.fs)
	, fa(node.fa)
{
	double h1, h2;
	if (node.center) {
		h1 = -node.height / 2.0;
		h2 = +node.height / 2.0;
	}
	else {
		h1 = 0;
		h2 = node.height;
	}
	// z is based on the input time so height is correct
	z = (h2 - h1) * time + h1;

	// current time
	t = time;
	// inverse time
	double it = 1 - t;

	//double t0 = node.t0;
	//double t1 = node.t1;
	//double tt = (t1 - t0) * t + t0;
	//double tit = (t1 - t0) * it + t0;
	//
	//// hyperbolic limit
	//double ht0 = 1 - sqrt(1 - node.t0 * node.t0);
	//double ht1 = sqrt(1 - node.t1 * node.t1);
	//
	//// normalized hyperbolic time
	//double htt = (1 - ht1 - ht0) * t + ht0;
	//double hit = (1 - ht1 - ht0) * it + ht0;

	// hyperbolic time
	double rt = 1 - sqrt(1 - t * t);
	double irt = sqrt(1 - it * it);

	auto r0 = node.twist * t;
	auto r1 = node.rtwist * rt;
	auto r2 = node.irtwist * irt;
	rot = r0 + r1 + r2;

	auto vscale = Vector2d(1 - (1 - node.vscale_x) * t, 1 - (1 - node.vscale_y) * t);
	auto rscale = Vector2d(1 - (1 - node.rscale_x) * rt, 1 - (1 - node.rscale_y) * rt);
	auto irscale = Vector2d(1 - (1 - node.irscale_x) * irt, 1 - (1 - node.irscale_y) * irt);
	auto scaleX = vscale[0] * rscale[0] * irscale[0];
	auto scaleY = vscale[1] * rscale[1] * irscale[1];
	auto scale = Vector2d(scaleX, scaleY);

	auto oscale = Vector2d(1 - (1 - node.scale_x) * t, 1 - (1 - node.scale_y) * t);
	auto origin = Vector2d(node.origin_x * oscale[0], node.origin_y * oscale[1]);

	vertTransform = 
		Eigen::Rotation2D<double>(-rot * M_PI / 180) *
		Eigen::Translation2d(origin) *
		Eigen::Affine2d(Eigen::Scaling(scale));

	scale0 = 
		scale[0] == 0 || scale[1] == 0 || 
		//sscale[0] == 0 || sscale[1] == 0 || 
		vertTransform.matrix().determinant() == 0;
}

SliceSettings::SliceSettings(double z)
	: fn(0)
	, fs(0)
	, fa(0)
	, t(0)
	, z(z)
	, rot(0)
	, scale0(false)
	, vertTransform(Eigen::Affine2d::Identity())
{
}

Vector2d SliceSettings::transformVert(const Vector2d &p) const {
	return vertTransform * p;
}

Vector2d SliceSettings::transformVert(const Vector3d &p) const {
	return transformVert(Vector2d(p[0], p[1]));
}

static void dumpAreas(const Polygon2d &poly)
{
	int i = 0;
	for (auto o : poly.outlines())
	{
		auto a = o.area();
		PRINTB("%s[%d].area=%f", (o.positive ? "Outline" : "Hole") % i % a);
		++i;
	}
}

// tesselates caps and adds the triangles transformed via ss to ps
static void add_cap(PolySet *ps, const Polygon2d &caps, const SliceSettings &ss, bool reverse)
{
	auto faces = std::shared_ptr<PolySet>(caps.tessellate());
	for (const auto &face : faces->getPolygons()) {
		Polygon p;
		for (const auto &v : face) {
			auto tv = ss.transformVert(v);
			p.push_back(Vector3d(tv[0], tv[1], ss.z));
		}
		if (reverse)
			std::reverse(p.begin(), p.end());
		ps->append_poly(p);
	}
}

// tesselates poly and adds the offset triangles to ps
static void add_poly(PolySet *ps, const Polygon2d &poly, const Vector3d &offset)
{
	auto polySet = std::shared_ptr<PolySet>(poly.tessellate());
	for (const auto &face : polySet->getPolygons()) {
		Polygon p;
		p.open = face.open;
		size_t c = face.size();
		for (size_t i = 0; i < c; ++i) {
			const auto &v = face[i];
			p.push_back(v + offset);
		}
		ps->append_poly(p);
	}
}

void SliceSettings::add_slice(PolySet *ps, const Outline2d &outlineA, const Outline2d &outlineB, const SliceSettings &settingsA, const SliceSettings &settingsB)
{
	bool splitfirst = sin((settingsA.rot - settingsB.rot) * M_PI / 180) < 0.0;
	Vector2d prevA = settingsA.transformVert(outlineA.vertices[0]);
	Vector2d prevB = settingsB.transformVert(outlineB.vertices[0]);
	bool positive = outlineA.positive;
	size_t numPoints = outlineA.vertices.size();
	for (size_t i = 1; i <= numPoints; i++) {
		if (i == numPoints && (outlineA.open || outlineB.open))
			break;
		size_t ti = i % numPoints;
		Vector2d currA = settingsA.transformVert(outlineA.vertices[ti]);
		Vector2d currB = settingsB.transformVert(outlineB.vertices[ti]);
		ps->append_poly();

		Vector3d prevBot = Vector3d(prevA[0], prevA[1], settingsA.z);
		Vector3d prevTop = Vector3d(prevB[0], prevB[1], settingsB.z);
		Vector3d currBot = Vector3d(currA[0], currA[1], settingsA.z);
		Vector3d currTop = Vector3d(currB[0], currB[1], settingsB.z);

		splitfirst = GeomUtils::splitfirst(prevBot, prevTop, currTop, currBot);

		// Make sure to split negative outlines correctly
		if (splitfirst xor !positive) {
			ps->insert_vertex(prevBot);
			ps->insert_vertex(currTop);
			ps->insert_vertex(currBot);
			if (!settingsB.scale0) {
				ps->append_poly();
				ps->insert_vertex(currTop);
				ps->insert_vertex(prevBot);
				ps->insert_vertex(prevTop);
			}
		}
		else {
			ps->insert_vertex(prevBot);
			ps->insert_vertex(prevTop);
			ps->insert_vertex(currBot);
			if (!settingsB.scale0) {
				ps->append_poly();
				ps->insert_vertex(prevTop);
				ps->insert_vertex(currTop);
				ps->insert_vertex(currBot);
			}
		}
		prevA = currA;
		prevB = currB;
	}
}

void SliceSettings::add_slice(PolySet *ps, const Outline2d &outline, const SliceSettings &settingsA, const SliceSettings &settingsB)
{
	add_slice(ps, outline, outline, settingsA, settingsB);
}

void SliceSettings::add_slice(PolySet *ps, const Polygon2d &polyA, const Polygon2d &polyB, const SliceSettings &settingsA, const SliceSettings &settingsB)
{
	for (size_t o = 0; o < polyA.outlines().size() && o < polyB.outlines().size(); ++o)
		add_slice(ps, polyA.outlines()[o], polyB.outlines()[o], settingsA, settingsB);
}

void SliceSettings::add_slice(PolySet *ps, const Polygon2d &poly, const SliceSettings &settingsA, const SliceSettings &settingsB)
{
	add_slice(ps, poly, poly, settingsA, settingsB);
}

FactoryModule<LinearExtrudeNode> LinearExtrudeNodeFactory("linear_extrude");
