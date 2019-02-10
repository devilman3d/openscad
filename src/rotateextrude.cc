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

#include "rotateextrudenode.h"
#include "FactoryNode.h"
#include "module.h"
#include "ModuleInstantiation.h"
#include "evalcontext.h"
#include "printutils.h"
#include "fileutils.h"
#include "builtin.h"
#include "polyset.h"
#include "PathHelpers.h"
#include "calc.h"
#include "clipper-utils.h"
#include "cgalutils.h"

#include <sstream>

class RotateExtrudeNode : public FactoryNode
{
	static void fill_ring(std::vector<Vector3d> &ring, const std::vector<Vector2d> &vertices, double a, double z, bool flip)
	{
		if (flip) {
			unsigned int l = vertices.size() - 1;
			for (unsigned int i = 0; i < vertices.size(); i++) {
				ring[i][0] = vertices[l - i][0] * sin(a);
				ring[i][1] = vertices[l - i][0] * cos(a);
				ring[i][2] = vertices[l - i][1] + z;
			}
		}
		else {
			for (unsigned int i = 0; i < vertices.size(); i++) {
				ring[i][0] = vertices[i][0] * sin(a);
				ring[i][1] = vertices[i][0] * cos(a);
				ring[i][2] = vertices[i][1] + z;
			}
		}
	}

	static double area2(const Vector3d &a, const Vector3d &b, const Vector3d &c)
	{
		Vector3d ab = b - a;
		Vector3d ac = c - a;
		double lab = ab.norm();
		double lac = ac.norm();
		if (std::isnan(lab) || std::isinf(lab) || lab < GRID_COARSE)
			return 1;
		if (std::isnan(lac) || std::isinf(lac) || lac < GRID_COARSE)
			return 1;
		return ab.dot(ac) / (lab * lac);
	}

	void getTransformMatrix(double t, Eigen::Affine2d &transform) const
	{
		double vs = 1 - (1 - vscale) * t;
		double ss = 1 - (1 - scale) * t;
		double tw = (twist * M_PI / 180) * t;
		Vector2d from_center = (centers[1] - centers[0]) * t + centers[0];
		Vector2d to_center = from_center * ss;
		Vector2d to_origin(origin_x, origin_y);

		transform = Eigen::Affine2d(
			Eigen::Translation2d(to_center + to_origin) *
			Eigen::Rotation2D<double>(tw) *
			Eigen::Scaling(vs, vs) *
			Eigen::Translation2d(-from_center));
	}

	/*!
		Input to extrude should be clean. This means non-intersecting, correct winding order
		etc., the input coming from a library like Clipper.

		FIXME: We should handle some common corner cases better:
	  o 2D polygon having an edge being on the Y axis:
		  In this case, we don't need to generate geometry involving this edge as it
			will be an internal edge.
	  o 2D polygon having a vertex touching the Y axis:
		This is more complex as the resulting geometry will (may?) be nonmanifold.
			In any case, the previous case is a specialization of this, so the following
			should be handled for both cases:
			Since the ring associated with this vertex will have a radius of zero, it will
			collapse to one vertex. Any quad using this ring will be collapsed to a triangle.

		Currently, we generate a lot of zero-area triangles

	*/
	Geometry *rotatePolygon(const Polygon2dHandles &polys) const
	{
		PolySet *ps = new PolySet(3);

		auto &node = *this;
		if (node.angle == 0)
			return ps;

		Polygon2d first_poly(*polys.front());
		Polygon2d last_poly(*polys.back());

		bool first = true;
		int fragments = 1;

		const BoundingBox &firstBB = first_poly.getBoundingBox();
		const BoundingBox &lastBB = last_poly.getBoundingBox();
		double min_x[2] = { firstBB.min()[0], lastBB.min()[0] };
		double max_x[2] = { firstBB.max()[0], lastBB.max()[0] };
		double min_y[2] = { firstBB.min()[1], lastBB.min()[1] };
		double max_y[2] = { firstBB.max()[1], lastBB.max()[1] };
		for (size_t t : { 0, 1 })
			if ((max_x[t] - min_x[t]) > max_x[t] && (max_x[t] - min_x[t]) > fabs(min_x[t])) {
				PRINTB("ERROR: all points for rotate_extrude() must have the same X coordinate sign (range is %.2f -> %.2f)", min_x[t] % max_x[t]);
				return ps;
			}

		for (size_t t : { 0, 1 })
			fragments = fmax(Calc::get_fragments_from_r(max_x[t] - min_x[t], node.fn, node.fs, node.fa) * std::abs(node.angle) / 360, fragments);

		bool flip_faces = false;
		for (size_t t : { 0, 1 })
			flip_faces |= (min_x[t] >= 0 && node.angle > 0 && node.angle != 360) || (min_x[t] < 0 && (node.angle < 0 || node.angle == 360));

		for (size_t t : { 0, 1 })
			centers[t] = Vector2d((max_x[t] + min_x[t]) / 2, (max_y[t] + min_y[t]) / 2);

		Polygon2d start_poly(first_poly);
		Eigen::Affine2d startTrans;
		getTransformMatrix(0, startTrans);
		start_poly.transform(startTrans);

		if (std::abs(node.angle) != 360 || node.vscale != 1 || node.attack != 0) {
			// starting face
			{
				PolySet *ps_start = start_poly.tessellate();
				Transform3d rot(Eigen::AngleAxisd(M_PI / 2, Vector3d::UnitX()));
				ps_start->transform(rot);
				// Flip vertex ordering
				if (!flip_faces) {
					for (Polygon &p : ps_start->getPolygons()) {
						std::reverse(p.begin(), p.end());
					}
				}
				ps->append(*ps_start);
				delete ps_start;
			}

			// ending face
			{
				Polygon2d end_poly(last_poly);
				Eigen::Affine2d endvTrans;
				getTransformMatrix(1, endvTrans);
				end_poly.transform(endvTrans); // end

				PolySet *ps_end = end_poly.tessellate();
				Transform3d endTransform =
					Eigen::Translation3d(Vector3d(0, 0, node.attack)) *								// translate to the end Z
					Transform3d(Eigen::AngleAxisd(node.angle * M_PI / 180, Vector3d::UnitZ())) *	// rotate around Z to the end angle
					Transform3d(Eigen::AngleAxisd(M_PI / 2, Vector3d::UnitX()));					// rotate XY -> XZ
				ps_end->transform(endTransform);
				if (flip_faces) {
					for (Polygon &p : ps_end->getPolygons()) {
						std::reverse(p.begin(), p.end());
					}
				}
				ps->append(*ps_end);
				delete ps_end;
			}
		}

		for (size_t o = 0; o < first_poly.outlines().size(); ++o)
		{
			const auto &outline = first_poly.outlines()[o];
			size_t num_verts = outline.vertices.size();

			std::vector<Vector3d> rings[2];
			rings[0].resize(num_verts);
			rings[1].resize(num_verts);

			Eigen::Affine2d vTrans;
			getTransformMatrix(0, vTrans);
			Outline2d go;
			OutlineMorpher::generateRotatedOutline(outline, last_poly.outlines()[o], 0, vTrans, go);
			fill_ring(rings[0], go.vertices, M_PI / 2, 0, flip_faces); // first ring
			for (int j = 0; j < fragments; j++) {
				double t = ((double)j + 1) / fragments;
				double a = M_PI / 2 - t * (node.angle * M_PI / 180); // start on the X axis
				double z = node.attack * t;

				getTransformMatrix(t, vTrans);
				OutlineMorpher::generateRotatedOutline(outline, last_poly.outlines()[o], t, vTrans, go);
				fill_ring(rings[(j + 1) % 2], go.vertices, a, z, flip_faces);

				for (size_t i = 0; i < num_verts; i++) {
					auto pp = std::vector<Vector3d>();
					pp.push_back(rings[j % 2][i]);
					pp.push_back(rings[j % 2][(i + 1) % num_verts]);
					pp.push_back(rings[(j + 1) % 2][(i + 1) % num_verts]);
					pp.push_back(rings[(j + 1) % 2][i]);

					bool splitfirst = GeomUtils::splitfirst(pp[0], pp[1], pp[2], pp[3]);

					{ // first triangle
						Polygon tri0;
						if (splitfirst)
							tri0.push_back(pp[1]);
						else
							tri0.push_back(pp[0]);
						tri0.push_back(pp[2]);
						tri0.push_back(pp[3]);
						auto d0 = area2(tri0[0], tri0[1], tri0[2]);
						auto a0 = acos(d0) * 180 / M_PI;
						if (a0 > 0.1)
							ps->append_poly(tri0);
					}
					{ // second triangle
						Polygon tri1;
						if (splitfirst)
							tri1.push_back(pp[3]);
						else
							tri1.push_back(pp[2]);
						tri1.push_back(pp[0]);
						tri1.push_back(pp[1]);
						auto d1 = area2(tri1[0], tri1[1], tri1[2]);
						auto a1 = acos(d1) * 180 / M_PI;
						if (a1 > 0.1)
							ps->append_poly(tri1);
					}
				}
			}
		}
		return ps;
	}
public:
	RotateExtrudeNode() 
		: FactoryNode(
			"angle", "origin", "scale", "vscale", "twist", "morph", "attack",
			"$fn", "$fs", "$fa")
		, fn(0), fs(0), fa(0)
		, origin_x(0), origin_y(0), scale(1), angle(360), vscale(1), twist(0), attack(0)
		, morph(false), has_origin(false)
	{
	}

	double fn, fs, fa;
	double origin_x, origin_y, scale, angle, vscale, twist, attack;
	bool morph, has_origin;
	mutable Vector2d centers[2];
	mutable Vector2d origin;

	// Called by node GeometryEvaluator::visit to process child geometries.
	// Implement this method in derived classes to do something.
	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		Polygon2dHandles polygons;
		GeomUtils::collect(children, polygons);
		if (morph)
		{
			if (polygons.size() > 2)
				PRINT("WARNING: Rotate Extrude with morph only supports two polygons. The first and last will be used.");
		}
		else {
			if (polygons.size() > 1) {
				ClipperUtils utils;
				auto unioned = Polygon2dHandle(utils.apply(polygons, ClipperLib::ClipType::ctUnion));
				polygons.clear();
				polygons.push_back(unioned);
			}
		}
		if (!polygons.empty()) {
			auto rotated = rotatePolygon(polygons);
			assert(rotated);
			return ResultObject(rotated);
		}
		return ResultObject(new ErrorGeometry());
	}

	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		this->fn = c.lookup("$fn")->toDouble();
		this->fs = c.lookup("$fs")->toDouble();
		this->fa = c.lookup("$fa")->toDouble();

		ValuePtr angle = c.lookup("angle");
		ValuePtr scale = c.lookup("scale");
		ValuePtr vscale = c.lookup("vscale");
		ValuePtr twist = c.lookup("twist");
		ValuePtr origin = c.lookup("origin");
		ValuePtr morph = c.lookup("morph");
		ValuePtr attack = c.lookup("attack");

		angle->getFiniteDouble(this->angle);

		if (scale->getFiniteDouble(this->scale))
			if (this->scale < 0)
				this->scale = 1;

		if (vscale->getFiniteDouble(this->vscale))
			if (this->vscale < 0)
				this->vscale = 1;

		twist->getFiniteDouble(this->twist);

		if (origin->isDefined()) {
			has_origin = true;
			origin->getFiniteDouble(this->origin_x);
			origin->getFiniteDouble(this->origin_y);
			origin->getVec2(this->origin_x, this->origin_y);
		}

		this->morph = morph->toBool();

		if (attack->getFiniteDouble(this->attack)) {
			if (this->angle < -360)
				this->angle = -360;
			else if (this->angle > 360)
				this->angle = 360;
		}
	}
};

FactoryModule<RotateExtrudeNode> RotateExtrudeNodeFactory("rotate_extrude");
