#include "Polygon2d-CGAL.h"
#include "polyset.h"
#include "printutils.h"
#include "cgalutils.h"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Straight_skeleton_builder_2.h>
#include <CGAL/Polygon_offset_builder_2.h>
#include <CGAL/offset_polygon_2.h>
#include <CGAL/compute_outer_frame_margin.h>
#include <CGAL/arrange_offset_polygons_2.h>
#include <iostream>

namespace Polygon2DCGAL {

	struct FaceInfo
	{
		FaceInfo() : nesting_level(41) {}
		int nesting_level;
		bool in_domain() { return nesting_level % 2 == 1; }
	};
	struct VertInfo
	{
		VertInfo() : poly(nullptr), offset(-1), id(-1) {}
		const Polygon2d *poly;
		double offset;
		int id;
	};

	typedef CGAL::Exact_predicates_inexact_constructions_kernel			K;
	typedef CGAL::Triangulation_vertex_base_with_info_2<VertInfo, K>	Vbb;
	typedef CGAL::Triangulation_vertex_base_2<K, Vbb>					Vb;
	typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo, K>		Fbb;
	typedef CGAL::Constrained_triangulation_face_base_2<K, Fbb>			Fb;
	typedef CGAL::Triangulation_data_structure_2<Vb, Fb>				TDS;
	typedef CGAL::Exact_predicates_tag									Itag;
	typedef CGAL::Constrained_Delaunay_triangulation_2<K, TDS, Itag>	CDT;

	typedef CGAL::Exact_predicates_inexact_constructions_kernel					Kernel;
	typedef CGAL::Polygon_2<Kernel>												Contour;
	typedef boost::shared_ptr<Contour>											ContourPtr;
	typedef std::vector<ContourPtr>												ContourSequence;
	typedef CGAL::Straight_skeleton_2<Kernel>									Ss;
	typedef CGAL::Straight_skeleton_builder_traits_2<Kernel>					SsBuilderTraits;
	typedef CGAL::Straight_skeleton_builder_2<SsBuilderTraits, Ss>				SsBuilder;
	typedef CGAL::Polygon_offset_builder_traits_2<Kernel>						OffsetBuilderTraits;
	typedef CGAL::Polygon_offset_builder_2<Ss, OffsetBuilderTraits, Contour>	OffsetBuilder;

	template <typename S, typename D>
	void convertPoints(const S &start, const S &end, std::vector<D> &result)
	{
		for (auto i = start; i != end; ++i)
			result.push_back(D(i->x(), i->y()));
	}

	void toPoints(const Outline2d &src, std::vector<Kernel::Point_2> &result)
	{
		convertPoints(src.vertices.begin(), src.vertices.end(), result);
	}

	std::vector<Kernel::Point_2> toPoints(const Outline2d &src)
	{
		std::vector<Kernel::Point_2> result;
		toPoints(src, result);
		return result;
	}

	void mark_domains(CDT &ct,
		CDT::Face_handle start,
		int index,
		std::list<CDT::Edge> &border)
	{
		if (start->info().nesting_level != -1) return;

		std::list<CDT::Face_handle> queue;
		queue.push_back(start);

		while (!queue.empty()) {
			CDT::Face_handle fh = queue.front();
			queue.pop_front();
			if (fh->info().nesting_level == -1) {
				fh->info().nesting_level = index;
				for (int i = 0; i < 3; i++) {
					CDT::Edge e(fh, i);
					CDT::Face_handle n = fh->neighbor(i);
					if (n->info().nesting_level == -1) {
						if (ct.is_constrained(e)) border.push_back(e);
						else queue.push_back(n);
					}
				}
			}
		}
	}

	// Explore set of facets connected with non constrained edges,
	// and attribute to each such set a nesting level.
	// We start from facets incident to the infinite vertex, with a nesting
	// level of 0. Then we recursively consider the non-explored facets incident 
	// to constrained edges bounding the former set and increase the nesting level by 1.
	// Facets in the domain are those with an odd nesting level.
	void mark_domains(CDT &cdt)
	{
		for (CDT::All_faces_iterator it = cdt.all_faces_begin(); it != cdt.all_faces_end(); ++it) {
			it->info().nesting_level = -1;
		}

		int index = 0;
		std::list<CDT::Edge> border;
		mark_domains(cdt, cdt.infinite_face(), index++, border);
		while (!border.empty()) {
			CDT::Edge e = border.front();
			border.pop_front();
			CDT::Face_handle n = e.first->neighbor(e.second);
			if (n->info().nesting_level == -1) {
				mark_domains(cdt, n, e.first->info().nesting_level + 1, border);
			}
		}
	}

	PolySet *tessellateRings(const Polygon2d &p0, const Polygon2d &p1, double z0, double z1)
	{
		PolySet *polyset = new PolySet(3);

		Polygon2DCGAL::CDT cdt; // Uses a constrained Delaunay triangulator.
		{
			CGALUtils::ErrorLocker errorLocker;
			try {
				auto pp = { &p0, &p1 };
				for (auto p : pp) {
					// Adds the spine of the skeleton as constraints.
					if (p->isEmpty()) {
						Skelegon2d skele(p == &p0 ? p1 : p0);
						auto ss = skele.skeleton;
						for (auto e = ss->halfedges_begin(); e != ss->halfedges_end(); ++e) {
							auto pv = e->vertex();
							auto nv = e->next()->vertex();
							if (pv->is_contour() || nv->is_contour())
								continue;
							Polygon2DCGAL::CDT::Vertex_handle prev = cdt.insert(pv->point());
							Polygon2DCGAL::CDT::Vertex_handle curr = cdt.insert(nv->point());
							if (prev != curr) { // Ignore duplicate vertices
								prev->info().poly = p;
								curr->info().poly = p;
								cdt.insert_constraint(prev, curr);
							}
						}
						continue;
					}
					// Adds all vertices, and add all contours as constraints.
					for (const auto &outline : p->outlines()) {
						// Start with last point
						auto lastPt = outline.vertices.back();
						Polygon2DCGAL::CDT::Vertex_handle prev = cdt.insert(CDT::Point(lastPt[0], lastPt[1]));
						prev->info().poly = p;
						for (const auto &v : outline.vertices) {
							Polygon2DCGAL::CDT::Vertex_handle curr = cdt.insert(CDT::Point(v[0], v[1]));
							if (prev != curr) { // Ignore duplicate vertices
								curr->info().poly = p;
								cdt.insert_constraint(prev, curr);
								prev = curr;
							}
						}
					}
				}
			}
			catch (const CGAL::Precondition_exception &e) {
				PRINTB("CGAL error in Polygon2d::tesselate(): %s", e.what());
				delete polyset;
				return NULL;
			}
		}

		// To extract triangles which is part of our polygon, we need to filter away
		// triangles inside holes.
		mark_domains(cdt);
		Polygon2DCGAL::CDT::Finite_faces_iterator fit = cdt.finite_faces_begin();
		for (; fit != cdt.finite_faces_end(); ++fit) {
			if (fit->info().in_domain()) {
				polyset->append_poly();
				for (int i = 0; i < 3; i++)
					polyset->append_vertex(
						fit->vertex(i)->point()[0],
						fit->vertex(i)->point()[1],
						fit->vertex(i)->info().poly == &p0 ? z0 : z1);
			}
		}
		return polyset;
	}

	PolySet *extractPolySet(CDT &cdt)
	{
		PolySet *polyset = new PolySet(3);
		Polygon2DCGAL::CDT::Finite_faces_iterator fit = cdt.finite_faces_begin();
		for (; fit != cdt.finite_faces_end(); ++fit) {
			auto nl = fit->info().nesting_level;
			if (fit->info().in_domain()) {
				polyset->append_poly();
				for (int i = 0; i < 3; i++)
					polyset->append_vertex(
						fit->vertex(i)->point()[0],
						fit->vertex(i)->point()[1],
						fit->vertex(i)->info().offset);
			}
		}
		return polyset;
	}

	void addToCDT(const Skelegon2d *p, int slices, double time, CDT &cdt)
	{
		// Adds the outermost outlines as constraints.
		for (const auto &c : p->outlines()) {
			// Start with last point
			auto lastPt = c.vertices.back();
			CDT::Vertex_handle prev = cdt.insert(CDT::Point(lastPt[0], lastPt[1]));
			prev->info().offset = 0;
			prev->info().id = 0;
			for (const auto &v : c.vertices) {
				CDT::Vertex_handle curr = cdt.insert(CDT::Point(v[0], v[1]));
				curr->info().offset = 0;
				curr->info().id = 0;
				if (prev != curr) { // Ignore duplicate vertices
					cdt.insert_constraint(prev, curr);
					prev = curr;
				}
			}
		}

		mark_domains(cdt);

		// Store the previous contours
		ContourSequence prevContours;

		for (int i = 1; i < slices; ++i) {
			double a = M_PI_2 * i / slices;
			double ox = time * std::cos(a);
			double oy = time * std::sin(a);

			// Instantiate the container of offset contours
			ContourSequence contours;
			// Instantiate sm offset builder with the skeleton
			OffsetBuilder ob(*p->skeleton);
			// Obtain the offset contours
			ob.construct_offset_contours(ox, std::back_inserter(contours));

			if (contours.empty()) {
				break;
			}

			// Adds the contours as constraints.
			for (const auto &c : contours) {
				for (auto e = c->edges_begin(); e != c->edges_end(); ++e) {
					Polygon2DCGAL::CDT::Vertex_handle prev = cdt.insert(e->start());
					prev->info().offset = oy;
					prev->info().id = i;
					Polygon2DCGAL::CDT::Vertex_handle curr = cdt.insert(e->end());
					curr->info().offset = oy;
					curr->info().id = i;
					if (prev != curr) { // Ignore duplicate vertices
						cdt.insert_constraint(prev, curr);
					}
				}
			}

			prevContours = contours;
		}

		// Adds the spine of the last contours as constraints.
		if (!prevContours.empty())
		{
			Polygon2d poly;
			for (const auto &c : prevContours) {
				Outline2d outline;
				outline.positive = c->is_counterclockwise_oriented();
				convertPoints(c->vertices_begin(), c->vertices_end(), outline.vertices);
				poly.addOutline(outline);
			}
			Skelegon2d skele(poly);
			auto ss = skele.skeleton;
			for (auto e = ss->halfedges_begin(); e != ss->halfedges_end(); ++e) {
				auto pv = e->vertex();
				auto nv = e->next()->vertex();
				if (pv->is_contour() || nv->is_contour())
					continue; // Ignore off-spine edges
				Polygon2DCGAL::CDT::Vertex_handle prev = cdt.insert(pv->point());
				if (prev->info().id >= 0)
					continue;
				prev->info().offset = time;
				prev->info().id = slices;
				Polygon2DCGAL::CDT::Vertex_handle curr = cdt.insert(nv->point());
				if (curr->info().id >= 0)
					continue;
				curr->info().offset = time;
				curr->info().id = slices;
				if (prev != curr) { // Ignore duplicate vertices
					cdt.insert_constraint(prev, curr);
				}
			}
		}
	}

	void shrinkSkeleton(const Ss &ss, double offset, Polygon2d &result)
	{
		// Instantiate the container of offset contours
		ContourSequence offset_contours;
		// Instantiate sm offset builder with the skeleton
		OffsetBuilder ob(ss);
		// Obtain the offset contours
		ob.construct_offset_contours(offset, std::back_inserter(offset_contours));

		// generate result Polygon2d
		for (const auto &c : offset_contours) {
			Outline2d outline;
			outline.positive = c->is_counterclockwise_oriented();
			convertPoints(c->vertices_begin(), c->vertices_end(), outline.vertices);
			result.addOutline(outline);
		}
		result.setSanitized(true);
	}

	Polygon2d *shrinkSkeleton(const Polygon2d &poly, double offset)
	{
		Polygon2d result;

		// Instantiate the skeleton builder
		SsBuilder ssb;
		for (const auto &outline : poly.outlines()) {
			// Enter the outline (NOTE: the polygon's ordering should be correct)
			std::vector<Kernel::Point_2> star = toPoints(outline);
			ssb.enter_contour(star.begin(), star.end());
		}
		// Construct the skeleton
		boost::shared_ptr<Ss> ss = ssb.construct_skeleton();
		// Proceed only if the skeleton was correctly constructed.
		if (ss) {
			shrinkSkeleton(*ss, offset, result);
			return new Skelegon2d(result);
		}

		return nullptr;
	}

	Polygon2d *growSkeleton(const Polygon2d &poly, double offset)
	{
		typedef Kernel::Point_2 Point_2;

		std::vector<Point_2> cloud;
		for (const auto &outline : poly.outlines())
			toPoints(outline, cloud);

		// First we need to determine the proper separation between the polygon and the frame.
		// We use this helper function provided in the package.
		boost::optional<double> margin = CGAL::compute_outer_frame_margin(cloud.begin(), cloud.end(), offset);
		// Proceed only if the margin was computed (an extremely sharp corner might cause overflow)
		if (margin)
		{
			// Get the bbox of the polygon
			CGAL::Bbox_2 bbox = CGAL::bbox_2(cloud.begin(), cloud.end());
			// Compute the boundaries of the frame
			double fxmin = bbox.xmin() - *margin;
			double fxmax = bbox.xmax() + *margin;
			double fymin = bbox.ymin() - *margin;
			double fymax = bbox.ymax() + *margin;
			// Create the rectangular frame
			Point_2 frame[4] = { Point_2(fxmin,fymin)
							  , Point_2(fxmax,fymin)
							  , Point_2(fxmax,fymax)
							  , Point_2(fxmin,fymax)
			};
			// Instantiate the skeleton builder
			SsBuilder ssb;
			// Enter the frame
			ssb.enter_contour(frame, frame + 4);
			// Enter the polygon as a hole of the frame (NOTE: as it is a hole we insert it in the opposite orientation)
			for (const auto &outline : poly.outlines()) {
				std::vector<Point_2> star = toPoints(outline);
				ssb.enter_contour(star.rbegin(), star.rend());
			}
			// Construct the skeleton
			boost::shared_ptr<Ss> ss = ssb.construct_skeleton();
			// Proceed only if the skeleton was correctly constructed.
			if (ss)
			{
				// Instantiate the container of offset contours
				ContourSequence offset_contours;
				// Instantiate the offset builder with the skeleton
				OffsetBuilder ob(*ss);
				// Obtain the offset contours
				ob.construct_offset_contours(offset, std::back_inserter(offset_contours));
				// Locate the offset contour that corresponds to the frame
				// That must be the outmost offset contour, which in turn must be the one
				// with the largetst unsigned area.
				ContourSequence::iterator f = offset_contours.end();
				double lLargestArea = 0.0;
				for (ContourSequence::iterator i = offset_contours.begin(); i != offset_contours.end(); ++i)
				{
					double lArea = CGAL_NTS abs((*i)->area()); //Take abs() as  Polygon_2::area() is signed.
					if (lArea > lLargestArea)
					{
						f = i;
						lLargestArea = lArea;
					}
				}
				// Remove the offset contour that corresponds to the frame.
				offset_contours.erase(f);

				// generate result Polygon2d
				Polygon2d result;
				for (auto c : offset_contours) {
					Outline2d outline;
					outline.vertices.resize(c->size());
					outline.positive = !c->is_counterclockwise_oriented();
					for (size_t ii = 0; ii < c->size(); ++ii) {
						auto &cv = c->vertex(ii);
						auto vv = Vector2d(cv.x(), cv.y());
						outline.vertices[c->size() - 1 - ii] = vv;
					}
					result.addOutline(outline);
				}
				result.setSanitized(true);
				return new Skelegon2d(result);
			}
		}

		return nullptr;
	}

	void ringSkeleton(const Skelegon2d &ss, double r0, double r1, Polygon2d &result)
	{
		Polygon2d p0;
		if (r0 > 0)
			shrinkSkeleton(*ss.skeleton, r0, p0);
		else
			p0 = ss;
		Polygon2d p1;
		if (r1 > 0)
			shrinkSkeleton(*ss.skeleton, r1, p1);
		else
			p1 = ss;
		auto oneMinusR0 = ss.DIFF(p0);

		Geometry *geom = nullptr;
		if (r0 < r1) {
			// normal ring
			// intersect(r1, 1 - r0)
			result = p1.INTERSECT(oneMinusR0);
		}
		else if (r0 > r1) {
			// invert ring
			// union(r1, 1 - r0)
			result = p1.UNION(oneMinusR0);
		}
		else {
			// r0 == r1 => nothing
		}
	}
}

Skelegon2d::Skelegon2d(const Polygon2d &poly, boost::shared_ptr<Ss> ss)
	: Polygon2d(poly)
{
	typedef CGAL::Straight_skeleton_builder_traits_2<Kernel>      SsBuilderTraits;
	typedef CGAL::Straight_skeleton_builder_2<SsBuilderTraits, Ss> SsBuilder;
	typedef CGAL::Polygon_offset_builder_traits_2<Kernel>                  OffsetBuilderTraits;
	typedef CGAL::Polygon_offset_builder_2<Ss, OffsetBuilderTraits, Contour> OffsetBuilder;
	typedef Contour::Point_2 Point_2;

	this->type = "Skelegon";
	this->data.skelegon = this;
	if (ss) {
		// reference the input skeleton
		this->skeleton = ss;
	}
	else if (const auto skele = dynamic_cast<const Skelegon2d*>(&poly)) {
		// reference the existing skeleton
		this->skeleton = skele->skeleton;
	}
	else {
		// Instantiate a skeleton builder
		SsBuilder ssb;
		for (const auto &outline : outlines()) {
			// Enter the outline (NOTE: the polygon's ordering should be correct)
			std::vector<Point_2> star = Polygon2DCGAL::toPoints(outline);
			ssb.enter_contour(star.begin(), star.end());
		}
		// Construct the skeleton
		this->skeleton = ssb.construct_skeleton();
	}
}

/*!
	Triangulates this polygon2d and returns a 2D PolySet.
*/
PolySet *Polygon2d::tessellate() const
{
	PRINTDB("Polygon2d::tessellate(): %d outlines", this->outlines().size());
	PolySet *polyset = new PolySet(*this);

	Polygon2DCGAL::CDT cdt; // Uses a constrained Delaunay triangulator.
	{
		CGALUtils::ErrorLocker errorLocker;
		try {
			// Adds all vertices, and add all contours as constraints.
			int oi = 0;
			for (const auto &outline : this->outlines()) {
				// pass polylines thru
				if (outline.open) {
					Polygon poly;
					poly.open = true;
					for (const auto &v : outline.vertices)
						poly.push_back(Vector3d(v[0], v[1], 0));
					polyset->append_poly(poly);
					continue;
				}
				// Start with last point
				auto lastPt = outline.vertices.back();
				Polygon2DCGAL::CDT::Vertex_handle prev = cdt.insert(Polygon2DCGAL::CDT::Point(lastPt[0], lastPt[1]));
				for (const auto &v : outline.vertices) {
					Polygon2DCGAL::CDT::Vertex_handle curr = cdt.insert(Polygon2DCGAL::CDT::Point(v[0], v[1]));
					if (prev != curr) { // Ignore duplicate vertices
						cdt.insert_constraint(prev, curr);
						prev = curr;
					}
				}
			}
		}
		catch (const CGAL::Precondition_exception &e) {
			PRINTB("CGAL error in Polygon2d::tesselate(): %s", e.what());
			delete polyset;
			return NULL;
		}
	}

	// To extract triangles which is part of our polygon, we need to filter away
	// triangles inside holes.
	mark_domains(cdt);
	Polygon2DCGAL::CDT::Finite_faces_iterator fit = cdt.finite_faces_begin();
	for (; fit != cdt.finite_faces_end(); ++fit) {
		if (fit->info().in_domain()) {
			polyset->append_poly();
			for (int i = 0; i < 3; i++)
				polyset->append_vertex(
					fit->vertex(i)->point()[0],
					fit->vertex(i)->point()[1],
					0);
		}
	}
	return polyset;
}

#include "FactoryNode.h"

class SkeletonNode : public FactoryNode
{
public:
	SkeletonNode() : FactoryNode("offset")
		, offset(0)
	{
	}

	void initialize(Context &ctx, const ModuleContext &evalctx) override
	{
		ctx.lookup("offset")->getDouble(offset);
	}

	virtual ResultObject visitChild(const Polygon2dHandle &child) const
	{
		if (offset < 0)
			return ResultObject(Polygon2DCGAL::shrinkSkeleton(*child, -offset));
		return ResultObject(Polygon2DCGAL::growSkeleton(*child, offset));
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		return visitChildren(children);
	}

	double offset;
};

class RingNode : public FactoryNode
{
	double _ir, _or, _it, _ot;

public:
	RingNode() : FactoryNode(
		"ir", "or",
		"it", "ot")
		, _ir(INFINITY)
		, _or(INFINITY)
		, _it(INFINITY)
		, _ot(INFINITY)
	{
	}

	void initialize(Context &ctx, const ModuleContext &evalctx) override
	{
		ctx.lookup("ir")->getDouble(_ir);
		ctx.lookup("or")->getDouble(_or);
		ctx.lookup("it")->getDouble(_it);
		ctx.lookup("ot")->getDouble(_ot);
	}

	virtual ResultObject visitChild(const Polygon2dHandle &child) const
	{
		Skelegon2d skele(*child);
		auto ss = skele.skeleton;

		// min/max time (0..t1)
		double t1 = 0;
		for (auto vi = ss->vertices_begin(); vi != ss->vertices_end(); ++vi)
			t1 = std::max(t1, vi->time());

		// user radii (0..t1)
		double _ir = std::isinf(this->_ir) ? 0 : this->_ir;
		double _or = std::isinf(this->_or) ? t1 : this->_or;
		double _rr = _or - _ir;

		// user time (0..1)
		double _it = std::isinf(this->_it) ? _ir / _rr : this->_it;
		double _ot = std::isinf(this->_ot) ? _or / _rr : this->_ot;

		// min/max radii (0..1) * rr
		double r0 = std::max(0.0, std::min(1.0, _it)) * _rr;
		double r1 = std::max(0.0, std::min(1.0, _ot)) * _rr;

		Polygon2d ring;
		Polygon2DCGAL::ringSkeleton(skele, t1 - r0, t1 - r1, ring);

		if (!ring.isEmpty())
			return ResultObject(ring.copy());

		return ResultObject();
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		return visitChildren(children);
	}
};

class RoofNode : public FactoryNode
{
	double r, height;
	int slices;
public:
	RoofNode() : FactoryNode("r", "height", "slices")
		, r(INFINITY)
		, height(INFINITY)
		, slices(1)
	{
	}

	void initialize(Context &ctx, const ModuleContext &evalctx) override
	{
		ctx.lookup("r")->getDouble(r);
		ctx.lookup("height")->getDouble(height);
		double slices = 5;
		ctx.lookup("slices")->getDouble(slices);
		this->slices = std::max(1, (int)slices);
	}

	virtual ResultObject visitChild(const Polygon2dHandle &child) const
	{
		PolySet *ps = new PolySet(3);
		// bottom face
		if (true)
		{
			auto psBot = child->tessellate();
			for (auto &tri : psBot->getPolygons())
				std::reverse(tri.begin(), tri.end());
			ps->append(*psBot);
			delete psBot;
		}
		// compute the skeleton
		Skelegon2d skele(*child);
		auto ss = skele.skeleton;

		// compute max time
		double zmax = 0;
		for (auto vi = ss->vertices_begin(); vi != ss->vertices_end(); ++vi)
			zmax = std::max(zmax, vi->time());

		double r = std::isinf(this->r) ? zmax : this->r;
		double height = std::isinf(this->height) ? zmax : this->height;

		double t0 = 0;
		Polygon2d p0 = *child;
		for (int i = 0; i < slices; ++i) {
			double t1 = (double)(i + 1) / slices;
			double r0 = t0 * zmax;
			double r1 = t1 * zmax;
			// create a pair of insets at r0, r1
			Polygon2d *p1 = Polygon2DCGAL::shrinkSkeleton(*child, r1);
			// create a skeleton of that ring
			//Polygon2DCGAL::tessellateRings(p0, p1)
			// for each border edge in r0, add cdt constraint w/ offset = height * t0
			// for each border edge in r1, add cdt constraint w/ offset = height * (t0 + (skeleton.t[point] / skeleton.tmax))
			// add triangles to ps using cdt
			//p0 = *p1;
			delete p1;
		}

		return ResultObject(ps);
	}

	virtual ResultObject processChildren(const NodeGeometries &children) const
	{
		return visitChildren(children);
	}
};

FactoryModule<SkeletonNode> SkeletonNodeFactory("skeleton");
FactoryModule<RoofNode> RoofNodeFactory("roof");
FactoryModule<RingNode> RingNodeFactory("ring");
