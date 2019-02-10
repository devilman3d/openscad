#pragma once

#include "Geometry.h"
#include "cgal.h"
#include "memory.h"
#include <string>
#include "linalg.h"

/*
	Geometry derived class to wrap CGAL_Nef_polyhedron3
*/
class CGAL_Nef_polyhedron : public Geometry
{
public:
	CGAL_Nef_polyhedron();
	CGAL_Nef_polyhedron(const shared_ptr<CGAL_Nef_polyhedron3> &p);
	CGAL_Nef_polyhedron(const shared_ptr<CGAL_Nef_polyhedron3> &p, const class PolySet &ps);
	CGAL_Nef_polyhedron(const CGAL_Nef_polyhedron3 &p);
	CGAL_Nef_polyhedron(const CGAL_Nef_polyhedron &src);
	~CGAL_Nef_polyhedron() { p3.reset(); }

	virtual size_t memsize() const;
	// FIXME: Implement, but we probably want a high-resolution BBox..
	virtual BoundingBox getBoundingBox() const;
	virtual std::string dump() const;
	virtual unsigned int getDimension() const { return 3; }
  // Empty means it is a geometric node which has zero area/volume
	virtual bool isEmpty() const;
	virtual Geometry *copy() const { return new CGAL_Nef_polyhedron(*this); }

	void reset(CGAL_Nef_polyhedron3 *p3 = nullptr) { this->p3.reset(p3); }

	CGAL_Nef_polyhedron3 *get() const noexcept { return p3.get(); }

	CGAL_Nef_polyhedron3 *operator->() const noexcept { return p3.get(); }
	CGAL_Nef_polyhedron3 &operator*() const noexcept { return *p3; }

	CGAL_Nef_polyhedron *operator+(const CGAL_Nef_polyhedron &other) const;
	CGAL_Nef_polyhedron *intersection(const CGAL_Nef_polyhedron &other) const;
	CGAL_Nef_polyhedron *difference(const CGAL_Nef_polyhedron &other) const;
	CGAL_Nef_polyhedron *minkowski(const CGAL_Nef_polyhedron &other) const;
	//CGAL_Nef_polyhedron &operator+=(const CGAL_Nef_polyhedron &other);
	//CGAL_Nef_polyhedron &operator*=(const CGAL_Nef_polyhedron &other);
	//CGAL_Nef_polyhedron &operator-=(const CGAL_Nef_polyhedron &other);
	//CGAL_Nef_polyhedron &minkowski(const CGAL_Nef_polyhedron &other);

	void transform( const Transform3d &matrix );
	void resize(const Vector3d &newsize, const Eigen::Matrix<bool,3,1> &autosize);

private:
	shared_ptr<CGAL_Nef_polyhedron3> p3;
};
