#pragma once

#include "cgal.h"
#include "polyset.h"
#include "CGAL_Nef_polyhedron.h"
#include "enums.h"
#include "node.h"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
typedef CGAL::Epick K;
typedef CGAL::Point_3<K> Vertex3K;
typedef std::vector<Vertex3K> PolygonK;
typedef std::vector<PolygonK> PolyholeK;

#include <boost/smart_ptr/detail/spinlock.hpp>
namespace /* anonymous */ {
        template<typename Result, typename V>
        Result vector_convert(V const& v) {
                return Result(CGAL::to_double(v[0]),CGAL::to_double(v[1]),CGAL::to_double(v[2]));
       	}
}

namespace CGALUtils {
	typedef std::shared_ptr<const Geometry> GeometryHandle;
	typedef std::vector<GeometryHandle> GeometryHandles;

	bool applyHull(const GeometryHandles &children, PolySet &P);
	CGAL_Nef_polyhedron *applyOperator(const GeometryHandles &children, OpenSCADOperator op);
	//FIXME: Old, can be removed:
	//void applyBinaryOperator(CGAL_Nef_polyhedron &target, const CGAL_Nef_polyhedron &src, OpenSCADOperator op);
	Polygon2d *project(const CGAL_Nef_polyhedron &N, bool cut);
	shared_ptr<CGAL_Nef_polyhedron3> split(const CGAL_Nef_polyhedron &N, const CGAL_Nef_polyhedron3::Plane_3 &plane);
	CGAL_Iso_cuboid_3 boundingBox(const CGAL_Nef_polyhedron3 &N);
	bool is_approximately_convex(const PolySet &ps);
	Geometry const* applyMinkowski(const GeometryHandles &children);
	Geometry const* applyMinkowski(const Geometry *a, const Geometry *b);

	template <typename Polyhedron> std::string printPolyhedron(const Polyhedron &p);
	template <typename Polyhedron> bool createPolySetFromPolyhedron(const Polyhedron &p, PolySet &ps);
	template <typename Polyhedron> bool createPolyhedronFromPolySet(const PolySet &ps, Polyhedron &p);
	template <class Polyhedron_A, class Polyhedron_B> 
	void copyPolyhedron(const Polyhedron_A &poly_a, Polyhedron_B &poly_b);

	CGAL_Nef_polyhedron *createNefPolyhedronFromGeometry(const class Geometry &geom);
	PolySet *createPolySetFromNefPolyhedron(const CGAL_Nef_polyhedron &N);
	//bool createPolySetFromNefPolyhedron3(const CGAL_Nef_polyhedron3 &N, PolySet &ps);

	bool tessellatePolygon(const PolygonK &polygon,
						   Polygons &triangles,
						   const K::Vector_3 *normal = NULL);
	bool tessellatePolygonWithHoles(const PolyholeK &polygons,
									Polygons &triangles,
									const K::Vector_3 *normal = NULL);
	bool tessellate3DFaceWithHoles(std::vector<CGAL_Polygon_3> &polygons, 
								   std::vector<CGAL_Polygon_3> &triangles,
								   CGAL::Plane_3<CGAL_Kernel3> &plane);

	class ErrorLocker {
	public:
		ErrorLocker() {
			boost::detail::spinlock::scoped_lock lock(lockedErrorsLock);
			if (lockedErrorsCount++ == 0)
				oldBehavior = CGAL::set_error_behaviour(CGAL::THROW_EXCEPTION);
		}
		~ErrorLocker() {
			boost::detail::spinlock::scoped_lock lock(lockedErrorsLock);
			if (lockedErrorsCount > 0 && --lockedErrorsCount == 0)
				CGAL::set_error_behaviour(oldBehavior);
		}
	private:
		static boost::detail::spinlock lockedErrorsLock;
		static size_t lockedErrorsCount;
		static CGAL::Failure_behaviour oldBehavior;
	};
};
