#pragma once

#include "Geometry.h"
#include "linalg.h"
#include "grid.h"
#include "polyclipping/clipper.hpp"
#include <vector>

/*!
	A single contour.
	positive is (optionally) used to distinguish between polygon contours and hole contours.
	open is (optionally) used to indicate this contour is not closed, e.g. a polyline
*/
class Outline2d
{
public:
	std::vector<Vector2d> vertices;
	bool positive;
	bool open;

	Outline2d() : positive(true), open(false) {}

	double area() const;
	bool is_convex() const;
};

typedef Outline2d Polyline2d;

class Polygon2d : public Geometry
{
public:
	typedef std::vector<Outline2d> Outlines2d;

	Polygon2d() : sanitized(false) {
		type = "Polygon2d";
	}

	virtual size_t memsize() const;
	virtual BoundingBox getBoundingBox() const;
	virtual std::string dump() const;
	virtual unsigned int getDimension() const { return 2; }
	virtual bool isEmpty() const;
	virtual Geometry *copy() const { return new Polygon2d(*this); }

	void addOutline(const Outline2d &outline) { this->theoutlines.push_back(outline); }
	const Outlines2d &outlines() const { return theoutlines; }
	Outlines2d &outlines() { return theoutlines; }

	class PolySet *tessellate() const;

	void offset(double offset, ClipperLib::JoinType joinType, double fn, double fs, double fa);
	void transform(const Transform2d &mat);
	void resize(const Vector2d &newsize, const Eigen::Matrix<bool,2,1> &autosize);

	bool isSanitized() const { return this->sanitized; }
	void setSanitized(bool s) { this->sanitized = s; }
	bool is_convex() const;

	// union
	Polygon2d UNION(const Polygon2d &other, bool preserveCollinear = false) const;
	// subtraction
	Polygon2d DIFF(const Polygon2d &other, bool preserveCollinear = false) const;
	// xor
	Polygon2d XOR(const Polygon2d &other, bool preserveCollinear = false) const;
	// intersection
	Polygon2d INTERSECT(const Polygon2d &other, bool preserveCollinear = false) const;

	// union
	Polygon2d operator+(const Polygon2d &other) const;
	// subtraction
	Polygon2d operator-(const Polygon2d &other) const;
	// xor
	Polygon2d operator%(const Polygon2d &other) const;
	// intersection
	Polygon2d operator*(const Polygon2d &other) const;
private:
	Outlines2d theoutlines;
	bool sanitized;
};

struct PolygonIndex
{
	size_t polyIndex;
	size_t outlineIndex;
	size_t vertexIndex;

	PolygonIndex() : polyIndex(0), outlineIndex(0), vertexIndex(0) { }

	PolygonIndex(size_t polyIndex, size_t outlineIndex, size_t vertexIndex)
		: polyIndex(polyIndex), outlineIndex(outlineIndex), vertexIndex(vertexIndex)
	{
	}
};

typedef std::vector<PolygonIndex> PolygonIndices;

class PolygonIndexer : public Grid2d<PolygonIndices>
{
	std::vector<Polygon2d> polys;

public:
	PolygonIndexer()
	{
	}

	PolygonIndexer(const Polygon2d &poly)
	{
		addPolygon(poly);
	}

	void addPolygon(const Polygon2d &poly)
	{
		size_t pi = polys.size();
		polys.push_back(poly);
		Polygon2d &back = polys.back();
		for (size_t oi = 0, c = back.outlines().size(); oi < c; ++oi) {
			auto &vertices = back.outlines()[oi].vertices;
			for (size_t vi = 0, vc = vertices.size(); vi < vc; ++vi)
				align(vertices[vi]).push_back(PolygonIndex(pi, oi, vi));
		}
	}

	bool hasSeg(const Vector2d &v0, const Vector2d &v1)
	{
		PolygonIndices i0, i1;
		if (find(v0, i0) && find(v1, i1)) {
			for (const auto &ii0 : i0) {
				for (const auto &ii1 : i1) {
					if (ii0.polyIndex == ii1.polyIndex && ii0.outlineIndex == ii1.outlineIndex) {
						const auto &o = polys[ii0.polyIndex].outlines()[ii0.outlineIndex];
						if ((!o.positive && ii0.vertexIndex == ((ii1.vertexIndex + 1) % o.vertices.size())) ||
							(o.positive && ii1.vertexIndex == ((ii0.vertexIndex + 1) % o.vertices.size())))
							return true;
					}
				}
			}
		}
		return false;
	}

	void findPolylines(const Polygon2d &source, Polygon2d &result, bool invert)
	{
		for (const auto &o : source.outlines())
			findPolylines(o, result.outlines(), invert);
	}

	void findPolylines(const Outline2d &outline, std::vector<Outline2d> &plines, bool invert)
	{
		std::vector<Outline2d> result;
		const std::vector<Vector2d> &source = outline.vertices;
		// build a pline
		Outline2d working;
		working.positive = outline.positive;
		working.open = true;
		// true on the first iteration
		bool first = true;
		// was the first point marked?
		bool markedFirst = false;
		// iterate the points
		for (size_t i = 1; i <= source.size(); ++i) {
			const auto &v = source[i - 1];
			//const auto &v1 = source[i % source.size()];
			bool found = has(v);
			if (invert)
				found = !found;
			if (found) {
				if (first)
					markedFirst = true;
				working.vertices.push_back(v);
			}
			else if (!working.vertices.empty()) {
				// add the working pline
				result.push_back(working);
				// clear for the next pline
				working.vertices.clear();
			}
			first = false;
		}
		// add working pline
		if (!working.vertices.empty()) {
			if (markedFirst && result.size() > 0) {
				// append the first pline to the working pline
				working.vertices.insert(working.vertices.end(), result[0].vertices.begin(), result[0].vertices.end());
				// replace the first pline
				result[0] = working;
			}
			else if (markedFirst) {
				// found all the points
				//working.open = false;
				// "close" the pline
				if (!outline.open)// && hasSeg(working.vertices.back(), source.front()))
					working.vertices.push_back(source.front());
				result.push_back(working);
			}
			else // just add it
				result.push_back(working);
		}
		// update the output
		plines.insert(plines.end(), result.begin(), result.end());
	}
};