#pragma once

#include "polyclipping/clipper.hpp"
#include "Polygon2d.h"
#include "Handles.h"

class ClipperUtils {
public:
	static const unsigned int CLIPPER_SCALE = 1 << 16;

	bool preserveCollinear;

	ClipperUtils() : preserveCollinear(false) { }

	ClipperLib::Path fromOutline2d(const Outline2d &poly, bool keep_orientation);
	ClipperLib::Paths fromPolygon2d(const Polygon2d &poly);
	ClipperLib::PolyTree sanitize(const ClipperLib::Paths &paths);
	ClipperLib::Paths process(const ClipperLib::Paths &polygons, ClipperLib::ClipType, ClipperLib::PolyFillType);

	Polygon2d *sanitize(const Polygon2d &poly);
	Polygon2d *toPolygon2d(const ClipperLib::PolyTree &poly);
	Polygon2d *applyOffset(const Polygon2d& poly, double offset, ClipperLib::JoinType joinType, double miter_limit, double arc_tolerance);
	Polygon2d *applyMinkowski(const std::vector<const Polygon2d*> &polygons);
	Polygon2d *apply(const Polygon2dHandles &polygons, ClipperLib::ClipType clipType);
	Polygon2d *apply(const Polygon2ds &polygons, ClipperLib::ClipType clipType);
	Polygon2d *apply(const std::vector<ClipperLib::Paths> &pathsvector, ClipperLib::ClipType clipType);

	void sanitize(const Polygon2d &poly, Polygon2d &result);
	void toPolygon2d(const ClipperLib::PolyTree &poly, Polygon2d &result);
	void applyOffset(const Polygon2d& poly, double offset, ClipperLib::JoinType joinType, double miter_limit, double arc_tolerance, Polygon2d &result);
	void applyMinkowski(const std::vector<const Polygon2d*> &polygons, Polygon2d &result);
	void apply(const Polygon2dHandles &polygons, ClipperLib::ClipType clipType, Polygon2d &result);
	void apply(const Polygon2ds &polygons, ClipperLib::ClipType clipType, Polygon2d &result);
	void apply(const std::vector<ClipperLib::Paths> &pathsvector, ClipperLib::ClipType clipType, Polygon2d &result);
private:
	template <typename T>
	void _apply(const std::vector<T> &polygons, ClipperLib::ClipType clipType, Polygon2d &result);
};
