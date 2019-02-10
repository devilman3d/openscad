#pragma once

#ifdef ENABLE_CGAL

// NDEBUG must be disabled when including CGAL headers, otherwise CGAL assertions
// will not be thrown, causing OpenSCAD's CGAL error checking to fail.
// To be on the safe side, this has to be done when including any CGAL header file.
// FIXME: It might be possible to rewrite the error checking to get rid of this
// requirement. kintel 20111206.
#pragma push_macro("NDEBUG")
#undef NDEBUG

// define to force CGAL profiling information (comment to disable)
//#define FORCE_CGAL_PROFILE

// define to force CGAL non-reference-counted GMP types (comment to disable)
#define FORCE_CGAL_GMP_NO_REFCOUNT

// define to force CGAL replacement headers utilizing atomic operations (comment to disable)
//#define FORCE_CGAL_GMP_X

#ifdef FORCE_CGAL_PROFILE
#ifndef CGAL_PROFILE
#define CGAL_PROFILE_FORCED
#define CGAL_PROFILE
#endif
#endif // FORCE_CGAL_PROFILE

// replacement for CGAL/Profile_counter.h: concurrent profilers and time-profiler
#include "Profile_counterx.h"
// replacement for CGAL/Handle_for.h: concurrent handles
#include "CGAL_Handle_for_atomic_shared_ptr.h"	  // This file must be included prior to CGAL/Handle_for.h

#ifdef FORCE_CGAL_GMP_NO_REFCOUNT
#ifndef CGAL_GMPFR_NO_REFCOUNT
#define CGAL_GMPFR_NO_REFCOUNT
#endif
#ifndef CGAL_GMPQ_NO_REFCOUNT
#define CGAL_GMPQ_NO_REFCOUNT
#endif
#ifndef CGAL_GMPZ_NO_REFCOUNT
#define CGAL_GMPZ_NO_REFCOUNT
#endif
#ifndef CGAL_GMPZF_NO_REFCOUNT
#define CGAL_GMPZF_NO_REFCOUNT
#endif
#endif // FORCE_CGAL_GMP_NO_REFCOUNT

#ifdef FORCE_CGAL_GMP_X
// replacements for GMP wrapper-types utilizing atomic operations
#include "Gmpfrx_type.h"						  // This file must be included prior to CGAL/Gmpfr_type.h
#include "Gmpqx_type.h"							  // This file must be included prior to CGAL/Gmpq_type.h
#include "Gmpzx_type.h"							  // This file must be included prior to CGAL/Gmpz_type.h
#include "Gmpzfx_type.h"						  // This file must be included prior to CGAL/Gmpzf_type.h
#endif // FORCE_CGAL_GMP_X

// disable forced profiling
#ifdef CGAL_PROFILE_FORCED
#undef CGAL_PROFILE
#endif

#include "CGAL_workaround_Mark_bounded_volumes.h" // This file must be included prior to CGAL/Nef_polyhedron_3.h
#include <CGAL/Interval_nt.h>
#include <CGAL/Gmpq.h>
#include <CGAL/Extended_cartesian.h>
#include <CGAL/Nef_polyhedron_2.h>
#include <CGAL/Cartesian.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Nef_polyhedron_3.h>
#include "CGAL_Nef3_workaround.h"
#include <CGAL/IO/Polyhedron_iostream.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/minkowski_sum_2.h>
#include <CGAL/minkowski_sum_3.h>
#include <CGAL/bounding_box.h>
#include <CGAL/utils.h>

#include <CGAL/assertions_behaviour.h>
#include <CGAL/exceptions.h>

typedef CGAL::Gmpq NT2;
typedef CGAL::Extended_cartesian<NT2> CGAL_Kernel2;
typedef CGAL::Nef_polyhedron_2<CGAL_Kernel2> CGAL_Nef_polyhedron2;
typedef CGAL_Kernel2::Aff_transformation_2 CGAL_Aff_transformation2;

typedef CGAL::Exact_predicates_exact_constructions_kernel CGAL_ExactKernel2;
typedef CGAL::Polygon_2<CGAL_ExactKernel2> CGAL_Poly2;
typedef CGAL::Polygon_with_holes_2<CGAL_ExactKernel2> CGAL_Poly2h;

typedef CGAL::Gmpq NT3;
typedef CGAL::Cartesian<NT3> CGAL_Kernel3;

//typedef CGAL::Exact_predicates_exact_constructions_kernel::FT NT3;
typedef CGAL::Exact_predicates_inexact_constructions_kernel CGAL_Kepic;

//typedef CGAL::DefaultItems<CGAL_Kernel3> CGAL_Items3;

class CGAL_Nef_polyhedron3 : public CGAL::Nef_polyhedron_3<CGAL_Kernel3> {
	CGAL_Nef_polyhedron3(const SNC_structure& W, const SNC_point_locator* _pl);
public:
	CGAL_Nef_polyhedron3(Content space = Content::EMPTY);
	CGAL_Nef_polyhedron3(const CGAL::Nef_polyhedron_3<CGAL_Kernel3> &p);
	CGAL_Nef_polyhedron3(const CGAL_Nef_polyhedron3 &p);
	template <typename InputIterator>
	CGAL_Nef_polyhedron3(InputIterator begin, InputIterator end, Polylines_tag tag)
		: CGAL::Nef_polyhedron_3<CGAL_Kernel3>(begin, end, tag) {
		CGAL_assertion(this->unique() && "Created a non-unique polyline");
	}
	void copy_on_write() { CGAL::Nef_polyhedron_3<CGAL_Kernel3>::copy_on_write(); }
};

typedef CGAL_Nef_polyhedron3::Aff_transformation_3 CGAL_Aff_transformation;

typedef CGAL::Polyhedron_3<CGAL_Kernel3> CGAL_Polyhedron;

typedef CGAL::Point_3<CGAL_Kernel3> CGAL_Point_3;
typedef CGAL::Iso_cuboid_3<CGAL_Kernel3> CGAL_Iso_cuboid_3;
typedef std::vector<CGAL_Point_3> CGAL_Polygon_3;

// CGAL_Nef_polyhedron2 uses CGAL_Kernel2, but Iso_rectangle_2 needs to match
// CGAL_Nef_polyhedron2::Explorer::Point which is different than
// CGAL_Kernel2::Point. Hence the suffix 'e'
typedef CGAL_Nef_polyhedron2::Explorer::Point CGAL_Point_2e;
typedef CGAL::Iso_rectangle_2<CGAL::Simple_cartesian<NT2>> CGAL_Iso_rectangle_2e;

#pragma pop_macro("NDEBUG")

#endif /* ENABLE_CGAL */
