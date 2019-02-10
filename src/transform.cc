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

#include "transformnode.h"
#include "ModuleInstantiation.h"
#include "context.h"
#include "evalcontext.h"
#include "polyset.h"
#include "builtin.h"
#include "value.h"
#include "printutils.h"
#include "FactoryNode.h"
#include "clipper-utils.h"
#include "cgal.h"
#include "CGAL_Nef_polyhedron.h"
#include "progress.h"
#include "function.h"
#include <sstream>
#include <vector>
#include <assert.h>

template <transform_type_e TT>
class TransformFactoryNode : public TransformNode
{
public:
	template <typename ... Args>
	TransformFactoryNode(Args ... args) : TransformNode(TT, args...) { }

	bool isNoop() const { return this->matrix.isApprox(Transform3d::Identity()); }

	bool preferPoly() const override { return !isNoop(); }

	bool needsConversion(const NodeHandles &nodes) const
	{
		for (auto &node : nodes) {
			if (needsConversion(node))
				return true;
		}
		return false;
	}

	bool needsConversion(const NodeHandle &node) const
	{
		if (auto fn = dynamic_pointer_cast<FactoryNode>(node)) {
			if (fn->preferPoly())
				return false;
			if (fn->preferNef())
				return true;
		}
		return needsConversion(node->getChildren());
	}
	
	void addChild(const Context &c, const NodeHandle &child) override {
		if (isNoop()) {
			TransformNode::addChild(c, child);
			return;
		}
		if (needsConversion(child)) {
			auto polyNode = PolyNode::create(this->nodeFlags);
			polyNode->addChild(c, child);
			polyNode->setLocals(c);
			TransformNode::addChild(c, NodeHandle(polyNode));
		}
		else
			TransformNode::addChild(c, child);
	}

	ResultObject visitChild(const ConstPolySetHandle &child) const override
	{
		PolySet *newps = (PolySet*)child->copy();
		newps->transform(this->matrix);
		return ResultObject(newps);
	}

	ResultObject visitChild(const ConstNefHandle &child) const override
	{
		CGAL_Nef_polyhedron *newN = (CGAL_Nef_polyhedron*)child->copy();
		newN->transform(this->matrix);
		return ResultObject(newN);
	}

	ResultObject visitChild(const Polygon2dHandle &child) const override
	{
		auto newpoly = maybe_const<Polygon2d>((Polygon2d*)child->copy());
		Transform2d mat2;
		mat2.matrix() <<
			this->matrix(0, 0), this->matrix(0, 1), this->matrix(0, 3),
			this->matrix(1, 0), this->matrix(1, 1), this->matrix(1, 3),
			this->matrix(3, 0), this->matrix(3, 1), this->matrix(3, 3);
		newpoly.ptr()->transform(mat2);
		// A 2D transformation may flip the winding order of a polygon.
		// If that happens with a sanitized polygon, we need to reverse
		// the winding order for it to be correct.
		if (newpoly->isSanitized() && mat2.matrix().determinant() <= 0) {
			ClipperUtils utils;
			newpoly.reset(utils.sanitize(*newpoly));
		}
		return newpoly;
	}

	ResultObject processChildren(const NodeGeometries &children) const override
	{
		if (matrix_contains_infinity(this->matrix) || matrix_contains_nan(this->matrix)) {
			// due to the way parse/eval works we can't currently distinguish between NaN and Inf
			PRINT("WARNING: Transformation matrix contains Not-a-Number and/or Infinity - removing object.");
		}
		else if (isNoop()) {
			// identity matrix doesn't transform anything...
			return ResultObject(new GeometryGroup(children));
		}
		else {
			return visitChildren(children, CpuProgress::getCurrent());
		}
		return ResultObject(new EmptyGeometry());
	}

	void updateWorld(Context &context)
	{
		Transform3d curr(Matrix4d::Identity());
		if (!context.lookup("$world")->getTransform(curr))
			curr = Matrix4d::Identity();
		Transform3d world(curr * this->matrix);
		Transform3d invWorld(world.inverse());
		context.set_variable("$world", ValuePtr(world), false);
		context.set_variable("$invWorld", ValuePtr(invWorld), false);
	}
};

class ScaleNode : public TransformFactoryNode<transform_type_e::SCALE>
{
public:
	ScaleNode() : TransformFactoryNode("v") { }
	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		Vector3d scalevec(1, 1, 1);
		ValuePtr v = c.lookup_variable("v");
		if (!v->getVec3(scalevec[0], scalevec[1], scalevec[2], 1.0)) {
			double num;
			if (v->getDouble(num)) scalevec.setConstant(num);
		}
		this->matrix.scale(scalevec);
		updateWorld(c);
	}
};

FactoryModule<ScaleNode> ScaleNodeFactory("scale");

ValuePtr builtin_scaling(const Context *, const EvalContext *evalctx)
{
	if (evalctx->numArgs() == 1) {
		Vector3d scalevec(1, 1, 1);
		ValuePtr v = evalctx->getArgValue(0);
		if (!v->getVec3(scalevec[0], scalevec[1], scalevec[2], 1.0)) {
			double num;
			if (v->getDouble(num)) scalevec.setConstant(num);
		}
		Transform3d matrix(Transform3d::Identity());
		matrix.scale(scalevec);
		return ValuePtr(matrix);
	}
	return ValuePtr::undefined;
}

FactoryFunction BuiltinScaling("scaling", builtin_scaling);

class RotateNode : public TransformFactoryNode<transform_type_e::ROTATE>
{
public:
	RotateNode() : TransformFactoryNode("a", "v") { }
	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		ValuePtr val_a = c.lookup_variable("a");
		if (val_a->type() == Value::VECTOR)
		{
			Eigen::AngleAxisd rotx(0, Vector3d::UnitX());
			Eigen::AngleAxisd roty(0, Vector3d::UnitY());
			Eigen::AngleAxisd rotz(0, Vector3d::UnitZ());
			double a;
			if (val_a->toVector().size() > 0) {
				val_a->toVector()[0]->getDouble(a);
				rotx = Eigen::AngleAxisd(a*M_PI / 180, Vector3d::UnitX());
			}
			if (val_a->toVector().size() > 1) {
				val_a->toVector()[1]->getDouble(a);
				roty = Eigen::AngleAxisd(a*M_PI / 180, Vector3d::UnitY());
			}
			if (val_a->toVector().size() > 2) {
				val_a->toVector()[2]->getDouble(a);
				rotz = Eigen::AngleAxisd(a*M_PI / 180, Vector3d::UnitZ());
			}
			this->matrix.rotate(rotz * roty * rotx);
		}
		else
		{
			ValuePtr val_v = c.lookup_variable("v");
			double a = 0;

			val_a->getDouble(a);

			Vector3d axis(0, 0, 1);
			if (val_v->getVec3(axis[0], axis[1], axis[2])) {
				if (axis.squaredNorm() > 0) axis.normalize();
			}

			if (axis.squaredNorm() > 0) {
				this->matrix = Eigen::AngleAxisd(a*M_PI / 180, axis);
			}
		}
		updateWorld(c);
	}
};

FactoryModule<RotateNode> RotateNodeFactory("rotate");

ValuePtr builtin_rotation(const Context *, const EvalContext *evalctx)
{
	if (evalctx->numArgs() > 0) {
		Transform3d matrix(Transform3d::Identity());
		ValuePtr val_a = evalctx->getArgValue(0);
		if (val_a->type() == Value::VECTOR)
		{
			// rotate sequentially around the world axes
			Eigen::AngleAxisd rotx(0, Vector3d::UnitX());
			Eigen::AngleAxisd roty(0, Vector3d::UnitY());
			Eigen::AngleAxisd rotz(0, Vector3d::UnitZ());
			double a;
			if (val_a->toVector().size() > 0) {
				val_a->toVector()[0]->getDouble(a);
				rotx = Eigen::AngleAxisd(a*M_PI / 180, Vector3d::UnitX());
			}
			if (val_a->toVector().size() > 1) {
				val_a->toVector()[1]->getDouble(a);
				roty = Eigen::AngleAxisd(a*M_PI / 180, Vector3d::UnitY());
			}
			if (val_a->toVector().size() > 2) {
				val_a->toVector()[2]->getDouble(a);
				rotz = Eigen::AngleAxisd(a*M_PI / 180, Vector3d::UnitZ());
			}
			matrix.rotate(rotz * roty * rotx);
			return ValuePtr(matrix);
		}
		else
		{
			// rotate a degrees
			double a = 0;
			val_a->getDouble(a);

			// rotate around z-axis by default
			Vector3d axis(0, 0, 1);

			// check for an optional different axis
			ValuePtr val_v = evalctx->numArgs() > 1 ? evalctx->getArgValue(1) : ValuePtr::undefined;
			if (val_v->getVec3(axis[0], axis[1], axis[2])) {
				if (axis.squaredNorm() > 0)
					axis.normalize();
			}

			// rotate if a valid axis is specified
			if (axis.squaredNorm() > 0) {
				matrix = Eigen::AngleAxisd(a*M_PI / 180, axis);
				return ValuePtr(matrix);
			}
		}
	}
	return ValuePtr::undefined;
}

FactoryFunction BuiltinRotation("rotation", builtin_rotation);

class MirrorNode : public TransformFactoryNode<transform_type_e::MIRROR>
{
public:
	MirrorNode() : TransformFactoryNode("v") { }
	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		ValuePtr val_v = c.lookup_variable("v");
		double x = 1, y = 0, z = 0;

		if (val_v->getVec3(x, y, z)) {
			if (x != 0.0 || y != 0.0 || z != 0.0) {
				double sn = 1.0 / sqrt(x*x + y * y + z * z);
				x *= sn, y *= sn, z *= sn;
			}
		}

		if (x != 0.0 || y != 0.0 || z != 0.0)
		{
			Matrix4d m;
			m << 1 - 2 * x*x, -2 * y*x, -2 * z*x, 0,
				-2 * x*y, 1 - 2 * y*y, -2 * z*y, 0,
				-2 * x*z, -2 * y*z, 1 - 2 * z*z, 0,
				0, 0, 0, 1;
			this->matrix = m;
		}
		updateWorld(c);
	}
};

FactoryModule<MirrorNode> MirrorNodeFactory("mirror");

class TranslateNode : public TransformFactoryNode<transform_type_e::TRANSLATE>
{
public:
	TranslateNode() : TransformFactoryNode("v") { }
	TranslateNode(const Vector3d &v) : TransformFactoryNode("v")
	{
		this->matrix.translate(v);
	}
	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		ValuePtr v = c.lookup_variable("v");
		Vector3d translatevec(0, 0, 0);
		v->getVec3(translatevec[0], translatevec[1], translatevec[2]);
		this->matrix.translate(translatevec);
		updateWorld(c);
	}
};

FactoryModule<TranslateNode> TranslateNodeFactory("translate");

ValuePtr builtin_translation(const Context *, const EvalContext *evalctx)
{
	if (evalctx->numArgs() == 1) {
		ValuePtr v = evalctx->getArgValue(0);
		Vector3d translatevec(0, 0, 0);
		v->getVec3(translatevec[0], translatevec[1], translatevec[2]);
		Transform3d matrix(Transform3d::Identity());
		matrix.translate(translatevec);
		return ValuePtr(matrix);
	}
	return ValuePtr::undefined;
}

FactoryFunction BuiltinTranslation("translation", builtin_translation);

class CenterNode : public FactoryNode
{
public:
	CenterNode() : FactoryNode() { }

	ResultObject processChildren(const NodeGeometries &children) const override
	{
		BoundingBox bb;
		for (auto &child : children)
			bb.extend(child.second->getBoundingBox());
		TranslateNode ntrans(-bb.center());
		return ntrans.createGeometry(children);
	}
};

FactoryModule<CenterNode> CenterNodeFactory("center");

class MultmatrixNode : public TransformFactoryNode<transform_type_e::MULTMATRIX>
{
public:
	MultmatrixNode() : TransformFactoryNode("m") { }
	void initialize(Context &c, const ModuleContext &evalctx) override
	{
		ValuePtr v = c.lookup_variable("m");
		if (v->type() == Value::VECTOR) {
			Matrix4d rawmatrix = Matrix4d::Identity();
			for (int i = 0; i < 16; i++) {
				size_t x = i / 4, y = i % 4;
				if (y < v->toVector().size() && v->toVector()[y]->type() ==
					Value::VECTOR && x < v->toVector()[y]->toVector().size())
					v->toVector()[y]->toVector()[x]->getDouble(rawmatrix(y, x));
			}
			double w = rawmatrix(3, 3);
			if (w != 1.0) this->matrix = rawmatrix / w;
			else this->matrix = rawmatrix;
		}
		updateWorld(c);
	}
};

FactoryModule<MultmatrixNode> MultmatrixNodeFactory("multmatrix");
