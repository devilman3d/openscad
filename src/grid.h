#pragma once

#include "linalg.h"
#include "hash.h"
#include <boost/functional/hash.hpp>
#include <cmath>

#include <cstdint> // int64_t
#include <unordered_map>
#include <vector>
#include <utility>

//const double GRID_COARSE = 0.001;
//const double GRID_FINE   = 0.000001;
/* Using decimals that are exactly convertible to binary floating point 
(and then converted exactly to a GMPQ Rational that uses a small amount 
of bytes aka "limbs" in CGAL's engine) provides at least a 5% speedup 
for ctest -R CGAL. We choose 1/1024 and 1/(1024*1024) In python: print 
'%.64f' % float(fractions.Fraction(1,1024)) */
const double GRID_COARSE = 0.0009765625;
const double GRID_FINE   = 0.00000095367431640625;

namespace GridOffsets
{
	static constexpr int64_t Offsets2d[][2] =
	{
		{ 0, 0 },
		{ 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 },
		{ 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 }
	};

	static constexpr int NumOffsets2d = sizeof(Offsets2d) / sizeof(Offsets2d[0]);

	static constexpr int64_t Offsets3d[][3] =
	{
		{ 0, 0, 0 }, // center
		{ 1, 0, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1}, { 0, 0, -1 }, // axes
		{ 1, 1, 0 }, { -1, 1, 0 }, { -1, 1, 0 }, { 1, -1, 0 }, // horiz edges
		{ 0, 1, 1 }, { 0, 1, -1 }, { 0, -1, 1 }, { 0, -1, -1}, // vert edges
		{ 1, -1, 1 }, { -1, -1, 1 }, { 1, -1, -1 }, { -1, -1, -1 }, // front corners
		{ 1, 1, 1 }, { -1, 1, 1 }, { 1, 1, -1 }, { -1, 1, -1 }, // back corners
	};

	static constexpr int NumOffsets3d = sizeof(Offsets3d) / sizeof(Offsets3d[0]);
};

template <typename T>
class Grid2d
{
public:
	double res;
	typedef std::pair<int64_t, int64_t> Key;
	typedef size_t Index;
	std::unordered_map<Key, Index, boost::hash<Key>> db;
	std::vector<Vector2d> points;
	std::vector<T> values;

	Grid2d(double resolution = GRID_FINE) {
		res = resolution;
	}

	void clear() {
		db.clear();
		points.clear();
		values.clear();
	}

	bool isEmpty() const { return db.empty(); }

	/*!
		Aligns x,y to the grid or to existing point if one close enough exists.
		Returns the value stored if a point already existing or an uninitialized new value
		if not.
	*/
	T &align(double &x, double &y, const T &value = T()) {
		int64_t ix = int64_t(x / res);
		int64_t iy = int64_t(y / res);
		for (int i = 0; i < GridOffsets::NumOffsets2d; ++i) {
			int64_t jx = ix + GridOffsets::Offsets2d[i][0];
			int64_t jy = iy + GridOffsets::Offsets2d[i][1];
			auto found = db.find(std::make_pair(jx, jy));
			if (found != db.end()) {
				x = jx * res;
				y = jy * res;
				return values[found->second];
			}
		}
		x = ix * res;
		y = iy * res;
		db[std::make_pair(ix, iy)] = values.size();
		points.push_back(Vector2d(x, y));
		values.push_back(value);
		return values.back();
	}

	T &align(Vector2d &v, const T &value = T()) {
		return align(v[0], v[1], value);
	}

	T &align(const Vector2d &v, const T &value = T()) {
		Vector2d vv(v);
		return align(vv[0], vv[1], value);
	}

	T find(const Vector2d &v) const {
		return find(v[0], v[1]);
	}

	T find(double x, double y) const {
		T result;
		find(x, y, result);
		return result;
	}

	bool find(const Vector2d &v, T &value) const {
		return find(v[0], v[1], value);
	}

	bool find(double x, double y, T &value) const {
		int64_t ix = int64_t(x / res);
		int64_t iy = int64_t(y / res);
		for (int i = 0; i < GridOffsets::NumOffsets2d; ++i) {
			int64_t jx = ix + GridOffsets::Offsets2d[i][0];
			int64_t jy = iy + GridOffsets::Offsets2d[i][1];
			auto found = db.find(std::make_pair(jx, jy));
			if (found != db.end()) {
				value = values[found->second];
				return true;
			}
		}
		return false;
	}

	bool has(const Vector2d &v) const {
		return has(v[0], v[1]);
	}

	bool has(double x, double y) const {
		int64_t ix = int64_t(x / res);
		int64_t iy = int64_t(y / res);
		for (int i = 0; i < GridOffsets::NumOffsets2d; ++i) {
			int64_t jx = ix + GridOffsets::Offsets2d[i][0];
			int64_t jy = iy + GridOffsets::Offsets2d[i][1];
			auto found = db.find(std::make_pair(jx, jy));
			if (found != db.end())
				return true;
		}
		return false;
	}

	bool eq(double x1, double y1, double x2, double y2) {
		align(x1, y1);
		align(x2, y2);
		if (std::abs(x1 - x2) < res && std::abs(y1 - y2) < res)
			return true;
		return false;
	}

	T &data(double x, double y) {
		return align(x, y);
	}

	T &operator()(double x, double y) {
		return align(x, y);
	}
};

template <typename T>
class Grid3d
{
public:
	double res;
	typedef Vector3l Key;
	typedef std::unordered_map<Key, T> GridContainer;
	GridContainer db;
	std::vector<Vector3d> vec;
	
	Grid3d(const Grid3d &grid) : res(grid.res), db(grid.db) { }

	Grid3d(double resolution) : res(resolution) { }

	const Vector3d *getArray() {
		if (vec.empty()) {
			vec.resize(db.size());
			size_t i = 0;
			for (auto &pair : db) {
				const Key &key = pair.first;
				Vector3d &v = vec[i++];
				v[0] = (key[0] * this->res);
				v[1] = (key[1] * this->res);
				v[2] = (key[2] * this->res);
			}
		}
		return &vec[0];
	}

	inline void createGridVertex(const Vector3d &v, Vector3l &i) {
		i[0] = int64_t(v[0] / this->res);
		i[1] = int64_t(v[1] / this->res);
		i[2] = int64_t(v[2] / this->res);
	}

	// Aligns vertex to the grid. Returns index of the vertex.
	// Will automatically increase the index as new unique vertices are added.
	T align(Vector3d &v) {
		T data;
		align(v, data);
		return data;
	}
	bool align(Vector3d &v, T &data) {
		Vector3l key;
		createGridVertex(v, key);
		auto iter = db.find(key);
		if (iter == db.end()) {
			int64_t ix = key[0];
			int64_t iy = key[1];
			int64_t iz = key[2];
			for (int i = 1; i < GridOffsets::NumOffsets3d; ++i) {
				int64_t jx = ix + GridOffsets::Offsets3d[i][0];
				int64_t jy = iy + GridOffsets::Offsets3d[i][1];
				int64_t jz = iz + GridOffsets::Offsets3d[i][2];
				iter = db.find(Vector3l(jx, jy, jz));
				if (iter != db.end()) {
					break;
				}
			}
		}

		if (iter == db.end()) { // Not found: insert using key
			data = T(db.size());   // default data = (size_t)index
			db[key] = data;

			// Align vertex
			v[0] = (key[0] * this->res);
			v[1] = (key[1] * this->res);
			v[2] = (key[2] * this->res);

			vec.push_back(v);

			return false;
		}
		else {
			// If found return existing data
			key = iter->first;
			data = iter->second;

			// Align vertex
			v[0] = (key[0] * this->res);
			v[1] = (key[1] * this->res);
			v[2] = (key[2] * this->res);

			return true;
		}
	}

	bool has(const Vector3d &v, T *data = NULL) {
		Vector3l key = createGridVertex(v);
		int64_t ix = key[0];
		int64_t iy = key[1];
		int64_t iz = key[2];
		for (int i = 0; i < GridOffsets::NumOffsets3d; ++i) {
			int64_t jx = ix + GridOffsets::Offsets3d[i][0];
			int64_t jy = iy + GridOffsets::Offsets3d[i][1];
			int64_t jz = iz + GridOffsets::Offsets3d[i][2];
			auto pos = db.find(Vector3l(jx, jy, jz));
			if (pos != db.end()) {
				if (data) *data = pos->second;
				return true;
			}
		}
		return false;
	}

	T data(Vector3d v) {
		return align(v);
	}

};
