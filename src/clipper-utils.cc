#include "clipper-utils.h"
#include "printutils.h"

//const unsigned int ClipperUtils::CLIPPER_SCALE = 1 << 16;

ClipperLib::Path ClipperUtils::fromOutline2d(const Outline2d &outline, bool keep_orientation) {
	ClipperLib::Path p;
	for (const auto &v : outline.vertices) {
		p.push_back(ClipperLib::IntPoint(v[0] * CLIPPER_SCALE, v[1] * CLIPPER_SCALE));
	}
	// Make sure all polygons point up, since we project also 
	// back-facing polygon in PolysetUtils::project()
	if (!keep_orientation && !ClipperLib::Orientation(p)) std::reverse(p.begin(), p.end());

	return p;
}

ClipperLib::Paths ClipperUtils::fromPolygon2d(const Polygon2d &poly) {
	ClipperLib::Paths result;
	for (const auto &outline : poly.outlines()) {
		result.push_back(fromOutline2d(outline, poly.isSanitized() ? true : false));
	}
	return result;
}

ClipperLib::PolyTree ClipperUtils::sanitize(const ClipperLib::Paths &paths) {
	ClipperLib::PolyTree result;
	ClipperLib::Clipper clipper;
	clipper.PreserveCollinear(preserveCollinear);
	try {
		clipper.AddPaths(paths, ClipperLib::ptSubject, true);
	}
	catch (...) {
		// Most likely caught a RangeTest exception from clipper
		// Note that Clipper up to v6.2.1 incorrectly throws
				// an exception of type char* rather than a clipperException()
		PRINT("WARNING: Range check failed for polygon. skipping");
	}
	clipper.Execute(ClipperLib::ctUnion, result, ClipperLib::pftEvenOdd);
	return result;
}

ClipperLib::Paths ClipperUtils::process(const ClipperLib::Paths &polygons, ClipperLib::ClipType cliptype, ClipperLib::PolyFillType polytype)
{
	ClipperLib::Paths result;
	ClipperLib::Clipper clipper;
	clipper.PreserveCollinear(preserveCollinear);
	clipper.AddPaths(polygons, ClipperLib::ptSubject, true);
	clipper.Execute(cliptype, result, polytype);
	return result;
}

void ClipperUtils::sanitize(const Polygon2d &poly, Polygon2d &result)
{
	std::vector<const Outline2d*> openPaths;
	Polygon2d temp;
	for (const auto &outline : poly.outlines()) {
		if (outline.open) {
			openPaths.push_back(&outline);
			continue;
		}
		temp.addOutline(outline);
	}
	toPolygon2d(sanitize(fromPolygon2d(temp)), result);
	for (const auto *open : openPaths)
		result.addOutline(*open);
}

Polygon2d *ClipperUtils::sanitize(const Polygon2d &poly)
{
	Polygon2d *result = new Polygon2d;
	sanitize(poly, *result);
	return result;
}

/*!
	We want to use a PolyTree to convert to Polygon2d, since only PolyTrees
	have an explicit notion of holes.
	We could use a Paths structure, but we'd have to check the orientation of each
	path before adding it to the Polygon2d.
*/
void ClipperUtils::toPolygon2d(const ClipperLib::PolyTree &poly, Polygon2d &result)
{
	if (!result.isEmpty())
		result = Polygon2d();
	const double CLEANING_DISTANCE = 0.001 * CLIPPER_SCALE;
	const ClipperLib::PolyNode *node = poly.GetFirst();
	while (node) {
		Outline2d outline;
		outline.open = node->IsOpen();
		// Apparently, when using offset(), clipper gets the hole status wrong
		//outline.positive = !node->IsHole();
		outline.positive = Orientation(node->Contour);
		if (node->IsHole() == outline.positive)
		{
			PRINT("Found hole with opposite orientation");
		}
		//else
		{
			ClipperLib::Path cleaned_path;
			if (preserveCollinear)
				cleaned_path = node->Contour;
			else
				ClipperLib::CleanPolygon(node->Contour, cleaned_path, CLEANING_DISTANCE);

			// CleanPolygon can in some cases reduce the polygon down to no vertices
			if (cleaned_path.size() >= 3 || (outline.open && cleaned_path.size() >= 2)) {
				for (const auto &ip : cleaned_path) {
					Vector2d v(1.0*ip.X / CLIPPER_SCALE, 1.0*ip.Y / CLIPPER_SCALE);
					outline.vertices.push_back(v);
				}
				result.addOutline(outline);
			}
		}
		node = node->GetNext();
	}
	result.setSanitized(true);
}

Polygon2d *ClipperUtils::toPolygon2d(const ClipperLib::PolyTree &poly)
{
	Polygon2d *result = new Polygon2d;
	toPolygon2d(poly, *result);
	return result;
}

/*!
	Apply the clipper operator to the given paths.

 May return an empty Polygon2d, but will not return NULL.
 */
void ClipperUtils::apply(const std::vector<ClipperLib::Paths> &pathsvector, ClipperLib::ClipType clipType, Polygon2d &result)
{
	ClipperLib::Clipper clipper;
	clipper.PreserveCollinear(preserveCollinear);

	if (clipType == ClipperLib::ctIntersection && pathsvector.size() >= 2) {
		// intersection operations must be split into a sequence of binary operations
		ClipperLib::Paths source = pathsvector[0];
		ClipperLib::PolyTree tree;
		for (unsigned int i = 1; i < pathsvector.size(); i++) {
			clipper.AddPaths(source, ClipperLib::ptSubject, true);
			clipper.AddPaths(pathsvector[i], ClipperLib::ptClip, true);
			clipper.Execute(clipType, tree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
			if (i != pathsvector.size() - 1) {
				ClipperLib::PolyTreeToPaths(tree, source);
				clipper.Clear();
			}
		}
		toPolygon2d(tree, result);
	}
	else
	{
		bool first = true;
		for (const auto &paths : pathsvector) {
			clipper.AddPaths(paths, first ? ClipperLib::ptSubject : ClipperLib::ptClip, true);
			if (first) first = false;
		}
		ClipperLib::PolyTree sumresult;
		clipper.Execute(clipType, sumresult, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
		// The returned result will have outlines ordered according to whether 
		// they're positive or negative: Positive outlines counter-clockwise and 
		// negative outlines clockwise.
		toPolygon2d(sumresult, result);
	}
}

Polygon2d *ClipperUtils::apply(const std::vector<ClipperLib::Paths> &pathsvector, ClipperLib::ClipType clipType)
{
	Polygon2d *result = new Polygon2d;
	apply(pathsvector, clipType, *result);
	return result;
}

/*!
	  Apply the clipper operator to the given polygons.

	  May return an empty Polygon2d, but will not return NULL.
   */
template <typename T>
void ClipperUtils::_apply(const std::vector<T> &polygons, ClipperLib::ClipType clipType, Polygon2d &result)
{
	std::vector<const Outline2d*> openPaths;
	std::vector<ClipperLib::Paths> pathsvector;
	for (const auto &polygon : polygons) {
		Polygon2d temp;
		temp.setSanitized(polygon->isSanitized());
		for (const auto &outline : polygon->outlines()) {
			if (outline.open) {
				openPaths.push_back(&outline);
				continue;
			}
			temp.addOutline(outline);
		}
		ClipperLib::Paths polypaths = fromPolygon2d(temp);
		if (!temp.isSanitized()) 
			ClipperLib::PolyTreeToPaths(sanitize(polypaths), polypaths);
		pathsvector.push_back(polypaths);
	}
	apply(pathsvector, clipType, result);
	for (auto open : openPaths)
		result.addOutline(*open);
}

void ClipperUtils::apply(const Polygon2dHandles &polygons, ClipperLib::ClipType clipType, Polygon2d &result)
{
	_apply(polygons, clipType, result);
}

void ClipperUtils::apply(const Polygon2ds &polygons, ClipperLib::ClipType clipType, Polygon2d &result)
{
	_apply(polygons, clipType, result);
}

Polygon2d *ClipperUtils::apply(const Polygon2dHandles &polygons, ClipperLib::ClipType clipType)
{
	Polygon2d *result = new Polygon2d;
	apply(polygons, clipType, *result);
	return result;
}

Polygon2d *ClipperUtils::apply(const Polygon2ds &polygons, ClipperLib::ClipType clipType)
{
	Polygon2d *result = new Polygon2d;
	apply(polygons, clipType, *result);
	return result;
}

// This is a copy-paste from ClipperLib with the modification that the union operation is not performed
// The reason is numeric robustness. With the insides missing, the intersection points created by the union operation may
// (due to rounding) be located at slightly different locations than the original geometry and this
// can give rise to cracks
static void minkowski_outline(const ClipperLib::Path& poly, const ClipperLib::Path& path,
	ClipperLib::Paths& quads, bool isSum, bool isClosed)
{
	int delta = (isClosed ? 1 : 0);
	size_t polyCnt = poly.size();
	size_t pathCnt = path.size();
	ClipperLib::Paths pp;
	pp.reserve(pathCnt);
	if (isSum)
		for (size_t i = 0; i < pathCnt; ++i)
		{
			ClipperLib::Path p;
			p.reserve(polyCnt);
			for (size_t j = 0; j < poly.size(); ++j)
				p.push_back(ClipperLib::IntPoint(path[i].X + poly[j].X, path[i].Y + poly[j].Y));
			pp.push_back(p);
		}
	else
		for (size_t i = 0; i < pathCnt; ++i)
		{
			ClipperLib::Path p;
			p.reserve(polyCnt);
			for (size_t j = 0; j < poly.size(); ++j)
				p.push_back(ClipperLib::IntPoint(path[i].X - poly[j].X, path[i].Y - poly[j].Y));
			pp.push_back(p);
		}

	quads.reserve((pathCnt + delta) * (polyCnt + 1));
	for (size_t i = 0; i < pathCnt - 1 + delta; ++i)
		for (size_t j = 0; j < polyCnt; ++j)
		{
			ClipperLib::Path quad;
			quad.reserve(4);
			quad.push_back(pp[i % pathCnt][j % polyCnt]);
			quad.push_back(pp[(i + 1) % pathCnt][j % polyCnt]);
			quad.push_back(pp[(i + 1) % pathCnt][(j + 1) % polyCnt]);
			quad.push_back(pp[i % pathCnt][(j + 1) % polyCnt]);
			if (!Orientation(quad)) ClipperLib::ReversePath(quad);
			quads.push_back(quad);
		}
}

// Add the polygon a translated to an arbitrary point of each separate component of b.
// Ideally, we would translate to the midpoint of component b, but the point can
  // be chosen arbitrarily since the translated object would always stay inside
  // the minkowski sum. 
static void fill_minkowski_insides(const ClipperLib::Paths &a, const ClipperLib::Paths &b, ClipperLib::Paths &target)
{
	for (const auto &b_path : b) {
		// We only need to add for positive components of b
		if (!b_path.empty() && ClipperLib::Orientation(b_path) == 1) {
			const ClipperLib::IntPoint &delta = b_path[0]; // arbitrary point
			for (const auto &path : a) {
				target.push_back(path);
				for (auto &point : target.back()) {
					point.X += delta.X;
					point.Y += delta.Y;
				}
			}
		}
	}
}

void ClipperUtils::applyMinkowski(const std::vector<const Polygon2d*> &polygons, Polygon2d &result)
{
	if (polygons.size() == 1) {
		result = *polygons[0]; // Just copy
		return;
	}

	ClipperLib::Clipper c;
	c.PreserveCollinear(preserveCollinear);
	ClipperLib::Paths lhs = ClipperUtils::fromPolygon2d(*polygons[0]);

	for (size_t i = 1; i < polygons.size(); i++) {
		ClipperLib::Paths minkowski_terms;
		ClipperLib::Paths rhs = ClipperUtils::fromPolygon2d(*polygons[i]);

		// First, convolve each outline of lhs with the outlines of rhs
		for (auto const& rhs_path : rhs) {
			for (auto const& lhs_path : lhs) {
				ClipperLib::Paths result;
				minkowski_outline(lhs_path, rhs_path, result, true, true);
				minkowski_terms.insert(minkowski_terms.end(), result.begin(), result.end());
			}
		}

		// Then, fill the central parts
		fill_minkowski_insides(lhs, rhs, minkowski_terms);
		fill_minkowski_insides(rhs, lhs, minkowski_terms);

		// This union operation must be performed at each interation since the minkowski_terms
		// now contain lots of small quads
		c.Clear();
		c.AddPaths(minkowski_terms, ClipperLib::ptSubject, true);

		if (i != polygons.size() - 1)
			c.Execute(ClipperLib::ctUnion, lhs, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
	}

	ClipperLib::PolyTree polytree;
	c.Execute(ClipperLib::ctUnion, polytree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);

	toPolygon2d(polytree, result);
}

Polygon2d *ClipperUtils::applyMinkowski(const std::vector<const Polygon2d*> &polygons)
{
	Polygon2d *result = new Polygon2d;
	applyMinkowski(polygons, *result);
	return result;
}

void ClipperUtils::applyOffset(const Polygon2d& poly, double offset, ClipperLib::JoinType joinType, double miter_limit, double arc_tolerance, Polygon2d &result)
{
	ClipperLib::ClipperOffset co(miter_limit, arc_tolerance * CLIPPER_SCALE);
	for (const auto &outline : poly.outlines()) {
		ClipperLib::EndType endType = !outline.open ? ClipperLib::etClosedPolygon :
			joinType == ClipperLib::jtRound ? ClipperLib::etOpenRound : // r, chamfer = n/a
			joinType == ClipperLib::jtSquare ? ClipperLib::etOpenButt : // delta, chamfer = true
			ClipperLib::etOpenSquare; // ClipperLib::jtMiter // delta, chamfer = false
		co.AddPath(fromOutline2d(outline, poly.isSanitized()), joinType, endType);

	}
	ClipperLib::PolyTree tree;
	co.Execute(tree, offset * CLIPPER_SCALE);
	toPolygon2d(tree, result);
}

Polygon2d *ClipperUtils::applyOffset(const Polygon2d& poly, double offset, ClipperLib::JoinType joinType, double miter_limit, double arc_tolerance)
{
	Polygon2d *result = new Polygon2d;
	applyOffset(poly, offset, joinType, miter_limit, arc_tolerance, *result);
	return result;
}
