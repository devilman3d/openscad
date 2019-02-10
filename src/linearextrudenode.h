#pragma once

#include "linalg.h"

class PolySet;
class Outline2d;
class Polygon2d;
class LinearExtrudeNode;

struct SliceSettings {
	double fn, fs, fa;
	double t;
	double z;
	double rot;
	bool scale0;

	Eigen::Affine2d vertTransform;

	SliceSettings(double t, const LinearExtrudeNode &node);
	SliceSettings(double z);

	Vector2d transformVert(const Vector2d &p) const;
	Vector2d transformVert(const Vector3d &p) const;

	static void add_slice(PolySet *ps, const Outline2d &from, const Outline2d &to, const SliceSettings &bot, const SliceSettings &top);
	static void add_slice(PolySet *ps, const Outline2d &outline, const SliceSettings &bot, const SliceSettings &top);
	static void add_slice(PolySet *ps, const Polygon2d &from, const Polygon2d &to, const SliceSettings &bot, const SliceSettings &top);
	static void add_slice(PolySet *ps, const Polygon2d &poly, const SliceSettings &bot, const SliceSettings &top);
};