#pragma once

#include "Polygon2d.h"
#include "cgal.h"
#include <CGAL/Polygon_2.h>
#include <CGAL/Straight_skeleton_2.h>

class Skelegon2d : public Polygon2d
{
public:
	typedef CGAL::Exact_predicates_inexact_constructions_kernel       Kernel;
	typedef CGAL::Polygon_2<Kernel>    Contour;
	typedef boost::shared_ptr<Contour> ContourPtr;
	typedef std::vector<ContourPtr>    ContourSequence;
	typedef CGAL::Straight_skeleton_2<Kernel> Ss;

	Skelegon2d() { }
	explicit Skelegon2d(const Polygon2d &poly, boost::shared_ptr<Ss> ss = nullptr);
	virtual ~Skelegon2d() { }

	virtual Geometry *copy() const { return new Skelegon2d(*this); }

	boost::shared_ptr<Ss> skeleton;
};

namespace Polygon2DCGAL {
	Skelegon2d *createSkeleton(const Polygon2d &poly);

	Polygon2d *shrinkSkeleton(const Polygon2d &poly, double offset);
	Polygon2d *growSkeleton(const Polygon2d &poly, double offset);
};
