#pragma once

#include "cgal.h"
#include "Polygon2d.h"
#include "value.h"
#include "clipper-utils.h"

#include <vector>

class PathHelper
{
	double totalDistance;
	std::vector<Vector3d> segs;
	std::vector<Vector3d> points;

	void initDistances()
	{
		totalDistance = 0;
		segs.clear();
		if (points.size() > 1)
		{
			Vector3d prev(points.front());
			for (size_t i = 1; i < points.size(); ++i)
			{
				Vector3d diff = points[i] - prev;
				double dist = diff.norm();
				totalDistance += dist;
				segs.push_back(diff);
				prev = points[i];
			}
		}
	}

public:
	PathHelper(const Outline2d &points)
	{
		for (auto p : points.vertices)
			this->points.push_back(Vector3d(p.x(), p.y(), 0));
		auto p0 = points.vertices[0];
		this->points.push_back(Vector3d(p0.x(), p0.y(), 0));
		initDistances();
	}

	PathHelper(const std::vector<Vector3d> &points)
		: points(points)
	{
		initDistances();
	}

	PathHelper(const ValuePtr &path)
	{
		std::vector<Vector3d> _points;
		if (pointsFromValuePtr(path, _points))
			points = std::move(_points);
		initDistances();
	}

	CGAL_Nef_polyhedron3 *createPolyline3d()
	{
		std::vector<CGAL_Point_3> _points;
		for (auto p : points)
		{
			CGAL_Point_3 cgp(p.x(), p.y(), p.z());
			_points.push_back(cgp);
		}
		if (_points.empty())
		{
			return NULL;
		}

		// turn _points into a CGAL_Nef_polyhedron3 per: http://doc.cgal.org/latest/Minkowski_sum_3/index.html#Minkowski_sum_3Glide
		typedef CGAL_Point_3* point_iterator;
		typedef std::pair<point_iterator, point_iterator> point_range;
		typedef std::list<point_range> polyline;
		polyline poly;
		CGAL_Point_3 *data = _points.data();
		CGAL_Point_3 *data_end = data + _points.size();
		poly.push_back(point_range(data, data_end));
		CGAL_Nef_polyhedron3 *p3 = new CGAL_Nef_polyhedron3(poly.begin(), poly.end(), CGAL_Nef_polyhedron3::Polylines_tag());
		return p3;
	}

	static bool isPoints(const ValuePtr &inputPoints)
	{
		// definitely need a vector-type (list) to start
		if (!inputPoints->isDefinedAs(Value::ValueType::VECTOR))
			return false;

		auto pathVec = inputPoints->toVector();
		// bail if it is empty
		if (pathVec.empty())
			return false;

		auto pt0 = pathVec[0];
		// make sure the point is also a vector-type
		if (!pt0->isDefinedAs(Value::ValueType::VECTOR))
			return false;

		auto pt0_x = pt0[0];
		// make sure the point's values are a number-type
		if (!pt0_x->isDefinedAs(Value::ValueType::NUMBER))
			return false;

		// ok. it is a vector of number vectors
		return true;
	}

	static bool pointsFromValuePtr(const ValuePtr &inputPoints, std::vector<CGAL_Point_3> &points)
	{
		if (!isPoints(inputPoints))
			return false;

		auto pathVec = inputPoints->toVector();
		for (auto pathPoint : pathVec)
		{
			double x, y, z;
			pathPoint->getVec3(x, y, z);
			CGAL_Point_3 pt(x, y, z);
			points.push_back(pt);
		}
		return true;
	}

	static bool pointsFromValuePtr(const ValuePtr &inputPoints, std::vector<Vector3d> &points)
	{
		if (!isPoints(inputPoints))
			return false;

		auto pathVec = inputPoints->toVector();
		for (auto pathPoint : pathVec)
		{
			double x, y, z;
			pathPoint->getVec3(x, y, z);
			Vector3d pt(x, y, z);
			points.push_back(pt);
		}
		return true;
	}

	static bool indicesFromRange(RangeType range, std::vector<int> &indices)
	{
		if (range.numValues() == 0)
			return false;

		for (auto i = range.begin(); i != range.end(); ++i)
		{
			auto ii = *i;
			indices.push_back((int)ii);
		}
		return true;
	}

	static bool indicesFromVector(const std::vector<ValuePtr> &vec, std::vector<int> &indices)
	{
		// bail if it is empty
		if (vec.empty())
			return false;

		// handle a single path
		for (auto item : vec)
		{
			if (item->isDefinedAs(Value::ValueType::NUMBER))
			{
				double x = item->toDouble();
				indices.push_back((int)x);
			}
			else
			{
				PRINTB("Glide: '%s' is not a number", item->toString().c_str());
				return false;
			}
		}
		return true;
	}

	static bool indicesFromVector(const std::vector<ValuePtr> &vec, std::vector<std::vector<int>> &result)
	{
		// bail if it is empty
		if (vec.empty())
			return false;

		auto item0 = vec[0];
		if (item0->isDefinedAs(Value::ValueType::NUMBER))
		{
			// handle a single path
			std::vector<int> path;
			if (!indicesFromVector(vec, path))
				return false;
			result.push_back(path);
		}
		else
		{
			// handle multiple paths
			for (auto item : vec)
			{
				if (item->isDefinedAs(Value::ValueType::VECTOR))
				{
					std::vector<int> path;
					if (!indicesFromVector(item->toVector(), path))
						return false;
					result.push_back(path);
				}
				else if (item->isDefinedAs(Value::ValueType::RANGE))
				{
					std::vector<int> path;
					if (!indicesFromRange(item->toRange(), path))
						return false;
					result.push_back(path);
				}
				else
				{
					PRINTB("Glide: '%s' is not a range or list", item->toString().c_str());
					return false;
				}
			}
		}
		return true;
	}

	static bool indicesFromValuePtr(const ValuePtr &inputPoints, std::vector<std::vector<int>> &result)
	{
		if (inputPoints->isDefinedAs(Value::ValueType::VECTOR))
		{
			// handle a vector
			if (!indicesFromVector(inputPoints->toVector(), result))
				return false;
			return true;
		}
		else if (inputPoints->isDefinedAs(Value::ValueType::RANGE))
		{
			// handle a range
			std::vector<int> path;
			if (!indicesFromRange(inputPoints->toRange(), path))
				return false;
			result.push_back(path);
			return true;
		}
		return false;
	}
};

class PathHelpers
{
	std::vector<PathHelper> pathHelpers;

public:
	PathHelpers(const ValuePtr &points)
	{
		std::vector<Vector3d> _points;
		if (!PathHelper::pointsFromValuePtr(points, _points))
			return;
		PathHelper ph(_points);
		this->pathHelpers.push_back(ph);
	}

	PathHelpers(const ValuePtr &indices, const ValuePtr &points)
	{
		std::vector<Vector3d> _points;
		if (!PathHelper::pointsFromValuePtr(points, _points))
			return;
		std::vector<std::vector<int>> _paths;
		if (PathHelper::indicesFromValuePtr(indices, _paths))
		{
			for (auto _path : _paths)
			{
				std::vector<Vector3d> pathPoints;
				for (auto index : _path)
					pathPoints.push_back(_points[index]);
				PathHelper ph(pathPoints);
				this->pathHelpers.push_back(ph);
			}
		}
		else
		{
			PathHelper ph(_points);
			this->pathHelpers.push_back(ph);
		}
	}

	PathHelpers(const Polygon2d *poly)
	{
		for (auto o : poly->outlines())
		{
			PathHelper ph(o);
			this->pathHelpers.push_back(ph);
		}
	}

	size_t numPaths() const
	{
		return pathHelpers.size();
	}

	const PathHelper &getPath(int index) const
	{
		return pathHelpers[index];
	}

	std::vector<CGAL_Nef_polyhedron3*> createPolylines3d() const
	{
		std::vector<CGAL_Nef_polyhedron3*> result;
		for (auto pathHelper : pathHelpers)
		{
			auto poly = pathHelper.createPolyline3d();
			if (poly != NULL)
				result.push_back(poly);
		}
		return result;
	}

	static std::vector<CGAL_Nef_polyhedron3*> createPolylines3d(const ValuePtr &indices, const ValuePtr &points)
	{
		PathHelpers instance(indices, points);
		return instance.createPolylines3d();
	}
};

class OutlineHelper
{
	const Outline2d &outline;
	Vector2d min, max;
public:
	OutlineHelper(const Outline2d &outline)
		: outline(outline)
	{
		bool first = true;
		for (auto v : outline.vertices)
		{
			min = first ? v : Vector2d(std::min(min[0], v[0]), std::min(min[1], v[1]));
			max = first ? v : Vector2d(std::max(max[0], v[0]), std::max(max[1], v[1]));
			first = false;
		}
	}

	Vector2d get_min() const { return min; }
	Vector2d get_max() const { return max; }
	Vector2d get_center() const { return (max + min) / 2.0; }

	std::vector<Vector2d> rotate_around(Vector2d pt, double angle) const
	{
		auto rot = Eigen::Rotation2Dd(angle);
		std::vector<Vector2d> result;
		for (auto v : outline.vertices)
		{
			Vector2d tx = (rot * (v - pt)) + pt;
			result.push_back(tx);
		}
		return result;
	}
};
// indicates the "side" for caps
// subtract 2 = [-1:0:1] = normalized center
// subtract 1, divide 2.0 = [0:0.5:1] = normalized unit
enum CapSide
{
	CAP_FROM = 1,
	CAP_MID = 2,
	CAP_TO = 3, 
	CAP_BOTH = 4
};

struct CapInfo
{
	// the side
	CapSide side;
	// the maximum depth [subdivision] level
	int maxDepth;
	// the current depth level
	int depth;
	// the index at the current depth
	int index;
	// the "t0" value for the cap
	double t0;
	// the "t1" value for the cap
	double t1;
};

struct CapVertex
{
	CapSide side;
	Vector2d v;

	CapVertex(CapSide side, double x, double y) : side(side), v(x, y) { }
};

typedef std::vector<CapVertex> CapFace;

struct CapFaces
{
	// the info for the contained faces
	CapInfo info;
	// the faces at info
	std::vector<CapFace> faces;

	CapFaces(const CapInfo &info) : info(info) { }
};

struct PolygonCaps
{
	const Polygon2d &fromPoly;
	const Polygon2d &toPoly;

	PolygonIndexer fromGrid;
	PolygonIndexer toGrid;
	PolygonIndexer fromEdgesGrid;
	PolygonIndexer toEdgesGrid;

	Polygon2d midway;

	Polygon2d fromCaps;
	Polygon2d toCaps;
	Polygon2d fromEdges;
	Polygon2d toEdges;
	Polygon2d fromBaseEdges;
	Polygon2d toBaseEdges;

	PolygonCaps(const Polygon2d &fromPoly, const Polygon2d &toPoly)
		: fromPoly(fromPoly)
		, toPoly(toPoly)
		, fromGrid(fromPoly)
		, toGrid(toPoly)
		, midway(fromPoly.XOR(toPoly))
		, fromCaps(toPoly - fromPoly)
		, toCaps(fromPoly - toPoly)
	{
		// find outlines in toCaps not in fromGrid -> toEdges
		fromGrid.findPolylines(toCaps, toEdges, true);
		// find outlines in fromCaps not in toGrid -> fromEdges
		toGrid.findPolylines(fromCaps, fromEdges, true);

		fromEdgesGrid.addPolygon(fromEdges);
		toEdgesGrid.addPolygon(toEdges);

		// find outlines in fromCaps not in fromGrid -> toBaseEdges
		fromGrid.findPolylines(fromCaps, toBaseEdges, true);
		// find outlines in toCaps not in toGrid -> fromBaseEdges
		toGrid.findPolylines(toCaps, fromBaseEdges, true);

		fromEdgesGrid.addPolygon(fromBaseEdges);
		toEdgesGrid.addPolygon(toBaseEdges);
	}

	Polygon2d invert(const Polygon2d &poly, bool preserveCollinear = false)
	{
		Polygon2d temp;
		// only add the outermost outlines
		for (const auto &outline : poly.outlines()) {
			if (outline.positive)
				temp.addOutline(outline);
			else
				break;
		}
		return temp.DIFF(poly, preserveCollinear);
	}

	void markCapSides(Grid2d<CapSide> &grid, CapSide side, const Outline2d &outline) const
	{
		const auto &vp = outline.vertices;
		auto c = vp.size();
		for (size_t i = 0; i < c; ++i) {
			CapSide &value = grid.align(vp[i], side);
			if (value != side)
				value = CapSide::CAP_BOTH;
		}
	}

	void markCapSides(Grid2d<CapSide> &grid, CapSide side, const Polygon2d &poly) const
	{
		for (const auto &outline : poly.outlines())
			markCapSides(grid, side, outline);
	}

	std::vector<CapFace> getCapFaces(CapSide side) const
	{
		bool reverse = side == CAP_FROM;

		const Polygon2d &capsPoly = reverse ? fromCaps : toCaps;
		auto capsPolyset = shared_ptr<PolySet>(capsPoly.tessellate());

		std::vector<CapFace> sideFaces;
		if (true) {
			/* add faces for capsPolyset at correct height
			for (const auto &cap : capsPolyset->polygons) {
				CapFace face;
				size_t c = cap.size();
				for (size_t i = 0; i < c; ++i) {
					const auto &v = cap[i];// reverse ? cap[c - i - 1] : cap[i];
					bool foundFrom = fromGrid.has(v[0], v[1]);
					bool foundTo = toGrid.has(v[0], v[1]);
					if ((foundFrom && side == CAP_FROM) ||
						(foundTo && side == CAP_TO))
						face.push_back(CapVertex(side, v[0], v[1]));
					else
						face.push_back(CapVertex(CAP_MID, v[0], v[1]));
				}
				sideFaces.push_back(face);
			}
			*/
		}
		if (true) {
			/* add vertical faces for remaining hole segs
			const auto &holes = thruHoles;// side == CAP_FROM ? fromHoles : toHoles;
			for (const auto &o : holes.outlines()) {
				const auto &vp = o.vertices;
				auto c = vp.size();
				for (size_t i = 0; i <= c; ++i) {
					auto j = (i + 1) % c;
					auto h = (i + c - 1) % c;
					const auto &vi = vp[i];
					const auto &vj = vp[j];
					const auto &vh = vp[h];
					const auto &fromGrid = fromHoles.getGridPoints();
					const auto &toGrid = toHoles.getGridPoints();
					bool foundFromI = fromGrid.has(vi[0], vi[1]);
					bool foundFromJ = fromGrid.has(vj[0], vj[1]);
					bool foundFromH = fromGrid.has(vh[0], vh[1]);
					bool foundFrom = foundFromI || foundFromJ || foundFromH;
					bool foundToI = toGrid.has(vi[0], vi[1]);
					bool foundToJ = toGrid.has(vj[0], vj[1]);
					bool foundToH = toGrid.has(vh[0], vh[1]);
					bool foundTo = foundToI || foundToJ || foundToH;
					CapSide fromSide =
						(foundFrom) ? CAP_FROM :
						foundTo ? CAP_MID :
						CAP_MID;
					CapSide toSide =
						(foundTo) ? CAP_TO :
						foundFrom ? CAP_MID :
						CAP_MID;
					{
						CapFace face0;
						face0.push_back(CapVertex(fromSide, vi[0], vi[1]));
						face0.push_back(CapVertex(toSide, vi[0], vi[1]));
						face0.push_back(CapVertex(toSide, vj[0], vj[1]));
						sideFaces.push_back(face0);
						CapFace face1;
						face1.push_back(CapVertex(fromSide, vi[0], vi[1]));
						face1.push_back(CapVertex(toSide, vj[0], vj[1]));
						face1.push_back(CapVertex(fromSide, vj[0], vj[1]));
						sideFaces.push_back(face1);
					}
				}
			}
				*/
		}
		return sideFaces;
	}
};

class PolyCapper
{
public:

	const Polygon2d *fromPoly;
	const Polygon2d *toPoly;

	std::vector<CapFaces> subdivide(const PolygonCaps &caps, int maxDepth, int depth = 0, int index = 0)
	{
		std::vector<CapFaces> result;

		const Polygon2d &fromPoly = caps.fromPoly;
		const Polygon2d &toPoly = caps.toPoly;
		const Polygon2d &fromCaps = caps.fromCaps;
		const Polygon2d &toCaps = caps.toCaps;
		const Polygon2d &midway = caps.midway;

		if (depth < maxDepth) {
			// subdivide the next depth
			PolygonCaps from(fromPoly, midway);
			auto fromFaces = subdivide(from, maxDepth, depth + 1, index * 2);
			result.insert(result.end(), fromFaces.begin(), fromFaces.end());

			PolygonCaps to(midway, toPoly);
			auto toFaces = subdivide(to, maxDepth, depth + 1, index * 2 + 1);
			result.insert(result.end(), toFaces.begin(), toFaces.end());
		}
		else {
			// build the faces
			for (int i = 0; i < 2; ++i) {
				CapSide side = (CapSide)(i * 2 + 1);

				double t0 = (double)(index * 2 + i) / pow(2, maxDepth + 1);
				double t1 = (double)(index * 2 + i + 1) / pow(2, maxDepth + 1);

				CapInfo info = { side, maxDepth, depth, index * 2 + i, t0, t1 };
				CapFaces sideFaces(info);
				sideFaces.faces = caps.getCapFaces(side);

				result.push_back(sideFaces);
			}
		}

		return result;
	}

	PolygonCaps caps;
	std::vector<CapFaces> allCapFaces;

	PolyCapper(const Polygon2d *fromPoly, const Polygon2d *toPoly, int depth)
		: fromPoly(fromPoly)
		, toPoly(toPoly)
		, caps(*fromPoly, *toPoly)
	{
		allCapFaces = subdivide(caps, depth);
	}

	const Polygon2d &getPoly(bool top) const
	{
		if (top)
			return *toPoly;
		return *fromPoly;
	}

	BoundingBox getBoundingBox() const
	{
		BoundingBox result;
		result.extend(fromPoly->getBoundingBox());
		result.extend(toPoly->getBoundingBox());
		return result;
	}
};

class ContourOutlines
{
public:
	std::vector<const Outline2d*> outlines;
	std::vector<const Outline2d*> holes;
	size_t depth;

	bool empty() const
	{ 
		return outlines.empty() && holes.empty(); 
	}

	void clear()
	{
		outlines.clear();
		holes.clear();
	}
};

class PolygonContours : public std::vector<ContourOutlines>
{
public:
	PolygonContours() { }
	PolygonContours(const Polygon2d &poly)
	{
		bool positive = true;
		ContourOutlines working;
		for (const auto &outline : poly.outlines()) {
			if (outline.positive && !positive && !working.empty()) {
				push_back(working);
				working.clear();
				working.depth++;
			}
			positive = outline.positive;
			if (positive)
				working.outlines.push_back(&outline);
			else
				working.holes.push_back(&outline);
		}
		if (!working.empty())
			push_back(working);
	}
};

class OutlineMorpher
{
public:
	static size_t computeNumPoints(const Outline2d &a, const Outline2d &b)
	{
		size_t toNum = a.vertices.size();
		size_t fromNum = b.vertices.size();
		if (fromNum == toNum)
			return fromNum;
		size_t s = std::min(toNum, fromNum);
		size_t e = std::max(toNum, fromNum);
		for (size_t i = 1; i < e; ++i)
			if (((s * i) % e) == 0)
				return s * i;
		return fromNum * toNum;
	}

	static Vector2d pointOnOutline(const Outline2d &a, size_t i, size_t n)
	{
		// find the local point and the next one
		size_t numPoints = a.vertices.size();
		size_t fi = (i * numPoints / n);
		size_t fii = (fi + 1) % numPoints;
		Vector2d pointA = a.vertices[fi];
		Vector2d pointB = a.vertices[fii];
		// distribute this point between those two based on the number of points that should be between them
		size_t step = n / numPoints;
		size_t rem = i % step;
		double frac = (double)rem / step;
		Vector2d point = frac < 0.5 ? pointA : pointB;
		//Vector2d fromPoint = (fromPointB - fromPointA) * fracFrom + fromPointA;
		//PRINTB("From: rem=%d, step=%d, frac=%f", remFrom % stepFrom % fracFrom);
		//Vector2d fromPoint = from.outline.vertices[fi];
		return point;
	}

	static Vector2d pointOnOutline(const Outline2d &a, const Outline2d &b, size_t i, size_t n, double t)
	{
		Vector2d fromPoint = pointOnOutline(a, i, n);
		Vector2d toPoint = pointOnOutline(b, i, n);
		return (toPoint - fromPoint) * t + fromPoint;
	}

	static void generateOutline(const Outline2d &a, const Outline2d &b, double t, Outline2d &result)
	{
		size_t num_points = computeNumPoints(a, b);
		result.vertices.resize(num_points);
		for (size_t i = 0; i < num_points; ++i)
			result.vertices[i] = pointOnOutline(a, b, i, num_points, t);
	}

	static void generateRotatedOutline(const Outline2d &a, const Outline2d &b, double t, const Eigen::Affine2d &transform, Outline2d &result)
	{
		size_t num_points = computeNumPoints(a, b);
		result.vertices.resize(num_points);
		for (size_t i = 0; i < num_points; ++i)
		{
			Vector2d v = pointOnOutline(a, b, i, num_points, t);
			result.vertices[i] = transform * v;
		}
	}

	static void morphContoursToMany(const std::vector<const Outline2d*> &a, const std::vector<const Outline2d*> &b, double t, Polygon2d &result)
	{
		ClipperUtils utils;
		std::vector<ClipperLib::Paths> paths;
		for (const auto *ao : a) {
			for (const auto *bo : b) {
				Outline2d morphed;
				generateOutline(*ao, *bo, t, morphed);
				ClipperLib::Paths path;
				path.push_back(utils.fromOutline2d(morphed, false));
				paths.push_back(path);
			}
		}
		utils.apply(paths, ClipperLib::ClipType::ctUnion, result);
	}

	static void morphContours(const std::vector<const Outline2d*> &a, const std::vector<const Outline2d*> &b, double t, Polygon2d &result)
	{
		for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
			const Outline2d *ao = a[i];
			const Outline2d *bo = b[i];
			Outline2d morphed;
			morphed.positive = ao->positive;
			generateOutline(*ao, *bo, t, morphed);
			result.addOutline(morphed);
		}
	}

	static void morphContours(const std::vector<const Outline2d*> &a, const std::vector<const Outline2d*> &b, double t, const Eigen::Affine2d &transform, Polygon2d &result)
	{
		for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
			const Outline2d *ao = a[i];
			const Outline2d *bo = b[i];
			Outline2d morphed;
			morphed.positive = ao->positive;
			generateRotatedOutline(*ao, *bo, t, transform, morphed);
			result.addOutline(morphed);
		}
	}

	static void generatePolygon(const PolygonContours &a, const PolygonContours &b, double t, Polygon2d &result)
	{
		for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
			morphContours(a[i].outlines, b[i].outlines, t, result);
			morphContours(a[i].holes, b[i].holes, t, result);
		}
	}

	static void generateRotatedPolygon(const PolygonContours &a, const PolygonContours &b, double t, const Eigen::Affine2d &transform, Polygon2d &result)
	{
		for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
			morphContours(a[i].outlines, b[i].outlines, t, transform, result);
			morphContours(a[i].holes, b[i].holes, t, transform, result);
		}
	}
};


class PolyMorpher
{
public:
	const Polygon2d *fromPoly;
	const Polygon2d *toPoly;
	PolygonContours fromContours;
	PolygonContours toContours;

	PolyMorpher(const Polygon2d *fromPoly, const Polygon2d *toPoly)
		: fromPoly(fromPoly)
		, toPoly(toPoly)
		, fromContours(*fromPoly)
		, toContours(*toPoly)
	{
	}

	void generatePolygon(double t, Polygon2d &p) const
	{
		OutlineMorpher::generatePolygon(fromContours, toContours, t, p);
	}

	void generateRotatedPolygon(double t, const Eigen::Affine2d &transform, Polygon2d &p) const
	{
		OutlineMorpher::generateRotatedPolygon(fromContours, toContours, t, transform, p);
	}

	BoundingBox getBoundingBox() const
	{
		BoundingBox result;
		result.extend(fromPoly->getBoundingBox());
		result.extend(toPoly->getBoundingBox());
		return result;
	}
};
