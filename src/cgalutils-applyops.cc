// this file is split into many separate cgalutils* files
// in order to workaround gcc 4.9.1 crashing on systems with only 2GB of RAM

#ifdef ENABLE_CGAL

#include "cgalutils.h"
#include "polyset.h"
#include "printutils.h"
#include "Polygon2d.h"
#include "polyset-utils.h"
#include "grid.h"
#include "node.h"

#include "cgal.h"
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/normal_vector_newell_3.h>
#include <CGAL/Handle_hash_function.h>
#include <CGAL/Nef_nary_union_3.h>

#include <CGAL/config.h> 
#include <CGAL/version.h> 

// Apply CGAL bugfix for CGAL-4.5.x
#if CGAL_VERSION_NR > CGAL_VERSION_NUMBER(4,5,1) || CGAL_VERSION_NR < CGAL_VERSION_NUMBER(4,5,0) 
#include <CGAL/convex_hull_3.h>
#else
#include "convex_hull_3_bugfix.h"
#endif

#include "svg.h"
#include "Reindexer.h"
#include "GeometryUtils.h"
#include "progress.h"
#include "feature.h"

#include <map>
#include <unordered_set>
#include <vector>

#include <atomic>

// MinGW defines sprintf to libintl_sprintf which breaks usage of the
// Qt sprintf in QString. This is skipped if sprintf and _GL_STDIO_H
// is already defined, so the workaround defines sprintf as itself.
#ifdef __MINGW32__
#define _GL_STDIO_H
#define sprintf sprintf
#endif
#include <boost/thread.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

extern const char *OP_NAMES[];

namespace CGALUtils {
	// static definitions for ErrorLocker class
	boost::detail::spinlock ErrorLocker::lockedErrorsLock = BOOST_DETAIL_SPINLOCK_INIT;
	size_t ErrorLocker::lockedErrorsCount = 0;
	CGAL::Failure_behaviour ErrorLocker::oldBehavior = CGAL::ABORT;


	template<typename Polyhedron>
	bool is_weakly_convex(Polyhedron const& p) {
		for (typename Polyhedron::Edge_const_iterator i = p.edges_begin(); i != p.edges_end(); ++i) {
			typename Polyhedron::Plane_3 p(i->opposite()->vertex()->point(), i->vertex()->point(), i->next()->vertex()->point());
			if (p.has_on_positive_side(i->opposite()->next()->vertex()->point()) &&
				CGAL::squared_distance(p, i->opposite()->next()->vertex()->point()) > 1e-8) {
				return false;
			}
		}
		// Also make sure that there is only one shell:
		std::unordered_set<typename Polyhedron::Facet_const_handle, typename CGAL::Handle_hash_function> visited;
		// c++11
		// visited.reserve(p.size_of_facets());

		std::queue<typename Polyhedron::Facet_const_handle> to_explore;
		to_explore.push(p.facets_begin()); // One arbitrary facet
		visited.insert(to_explore.front());

		while (!to_explore.empty()) {
			typename Polyhedron::Facet_const_handle f = to_explore.front();
			to_explore.pop();
			typename Polyhedron::Facet::Halfedge_around_facet_const_circulator he, end;
			end = he = f->facet_begin();
			CGAL_For_all(he, end) {
				typename Polyhedron::Facet_const_handle o = he->opposite()->facet();

				if (!visited.count(o)) {
					visited.insert(o);
					to_explore.push(o);
				}
			}
		}

		return visited.size() == p.size_of_facets();
	}

	typedef shared_ptr<const CGAL_Nef_polyhedron> NefHandle;
	typedef shared_ptr<const CGAL_Nef_polyhedron3> Nef3Handle;

	class Threaded_Nef_nary_union_3 {
		typedef boost::detail::spinlock::scoped_lock scoped_lock;

		// forward
		struct Runner;

		// a wrapper to tie a node to parallized progress
		struct Item {
		public:
			const AbstractNode* node;
			const CGAL_Nef_polyhedron *poly;

			// creates an Item for the first depth
			Item(const AbstractNode* node, const CGAL_Nef_polyhedron *poly)
				: node(node)
				, poly(poly) {
			}

			// creates an Item for the next depth
			Item(const Runner *r)
				: node(r->g2.node)
				, poly(r->result) {
			}

			size_t progress_pct() const {
				return 0;// node->progress_mark * 100 / progress_report_count;
			}

			// comparison of Item pointers by progress_mark
			static bool compare_progress_mark(const Item& first, const Item& second)
			{
				return false;// first.node->progress_mark < second.node->progress_mark;
			}
		};

		// a wrapper for a threaded union of two Items
		struct Runner {
			boost::thread *thread;
			Item g1;
			Item g2;
			size_t depth;
			const CGAL_Nef_polyhedron *result;
			const CGAL::Failure_exception *e;

			Runner(const Item &g1, const Item &g2, size_t depth)
				: thread(nullptr)
				, g1(g1)
				, g2(g2)
				, depth(depth)
				, result(nullptr)
				, e(nullptr) {
			}

			void run_naked(bool last = false) {
				const Item &pg = last ? g2 : g1;
				PRINTB("[%1%] Uniting pair: %2%%% (%3%)",
					depth % pg.progress_pct() % pg.node->index());
				result = *g1.poly + *g2.poly;
			}

			void run() {
				try {
					run_naked();
				}
				catch (const CGAL::Failure_exception &e) {
					this->e = new CGAL::Failure_exception(e);
				}
			}

			template <class Func>
			void start(Func f) {
				thread = new boost::thread(f);
			}
		};

		// the items queued to be united
		std::vector<Item> queue;

		// Unions one iteration of sequential pairs in the queue.
		// The unioning of each pair happens on its own thread.
		// When finished, the queue will contain the unioned object(s).
		// Multiple calls to this method are required if there are 
		// more than two objects in the queue (happens in get_union).
		void unite(size_t depth) {
			bool odd = (depth & 1) == 0;
			// return if there is nothing to unite
			if (queue.size() < 2) {
				return;
			}
			// sort the queue to get ordered progress
			std::sort(queue.begin(), queue.end(), Item::compare_progress_mark);
			// short-circuit to a non-threaded union if there is only one pair
			if (queue.size() == 2) {
				Runner r(queue[0], queue[1], depth);
				// inner exceptions will throw!
				r.run_naked(true);
				// inner cancellations will throw!
				//r.g2.node->progress_report();
				// clear the used items from the queue
				queue.clear();
				// put the single item into the queue
				queue.push_back(Item(&r));
				return;
			}
			// the number of pairs in the queue
			size_t wait_count = queue.size() / 2;
			// a semaphore to signal thread completion
			boost::interprocess::interprocess_semaphore ready_event(0);
			// a list runners staged for queue insertion when done running
			std::list<Runner*> staged;
			// a list of the runners ready to be staged; updated by the thread
			std::list<Runner*> ready;
			// the runners currently running
			std::unordered_set<Runner*> running;
			// a spinlock to protect access to the "ready" runners
			boost::detail::spinlock ready_lock = BOOST_DETAIL_SPINLOCK_INIT;
			// indicates user-cancellation or an inner exception
			bool canceled = false;
			// the maximum threads to run at once
			size_t max_threads = boost::thread::hardware_concurrency();
			// the start of the queue
			auto start = queue.begin();
			// if this is an "odd" pass and an odd count of items,
			// skip one item to help with balancing
			if (odd && (queue.size() & 1)) {
				PRINTB("[%1%] Skipping odd union item", depth);
				start++;
			}
			// the first item to iterate
			auto iter1 = start;
			// the first item at the next depth
			auto iter2 = iter1 + wait_count;
			// the last item to iterate
			auto end = iter2;
			do {
				// create threads while not cancelled, maximized or out of items
				while (!canceled && running.size() < max_threads && iter1 < end) {
					Item g1 = *iter1++;
					Item g2 = *iter2++;
					Runner *r = new Runner(g1, g2, depth);
					running.insert(r);
					auto f = [r, &ready, &ready_event, &ready_lock] {
						r->run();
						scoped_lock lock(ready_lock);
						ready.push_back(r);
						ready_event.post();
					};
					r->start(f);
				}
				// wait for any runner(s) to complete
				ready_event.wait();
				// get the ready runners
				std::list<Runner*> ready_now;
				{
					scoped_lock lock(ready_lock);
					std::swap(ready_now, ready);
				}
				// stage the newly ready runners, report progress and check for exceptions
				for (auto r : ready_now) {
					try {
						//r->g1.node->progress_report();
					}
					catch (const ProgressCancelException &c) {
						canceled = true;
					}
					if (r->e)
						canceled = true;
					staged.push_back(r);
					running.erase(r);
				}
				// perform housekeeping if nothing is running
				if (running.empty()) {
					// check for cancellation or done, otherwise continue looping
					if (canceled || staged.size() == wait_count) {
						// clear the used items from the queue
						queue.erase(start, queue.end());
						// rethrow any inner exceptions
						const CGAL::Failure_exception *e = nullptr;
						// requeueing and cleanup
						for (auto r : staged) {
							if (r->e)
								e = r->e;
							// put a new item into the queue
							queue.push_back(Item(r));
							// cleanup...
							delete r->thread;
							delete r;
						}
						// rethrow any inner exceptions
						if (e)
							throw *e;
						// rethrow any inner cancellations
						if (canceled)
							throw ProgressCancelException();
					}
				}
				// continue looping if not done
			} while (staged.size() != wait_count);
		}

	public:
		void add_polyhedron(const AbstractNode *N, const CGAL_Nef_polyhedron &P) {
			queue.push_back(Item(N, &P));
		}

		const CGAL_Nef_polyhedron &get_union() {
			PRINTB("Uniting %1% items", queue.size());
			size_t depth = 1;
			do { unite(depth++); } while (queue.size() > 1);
			return *queue.front().poly;
		}
	};

	class CGAL_Nef_nary_union_3 : public CGAL::Nef_nary_union_3<CGAL_Nef_polyhedron3> {
	private:
		typedef CGAL::Nef_nary_union_3<CGAL_Nef_polyhedron3> Base;
	public:
		void add_polyhedron(const AbstractNode *N, const CGAL_Nef_polyhedron &P) {
			Base::add_polyhedron(*P);
		}
	};

	/*!
		Applies UNION to all children and returns the result.
	*/
	template <class T>
	CGAL_Nef_polyhedron *applyUnion(T &nary_union, const GeometryHandles &children)
	{
		bool error = true;
		std::vector<shared_ptr<const CGAL_Nef_polyhedron>> pointers;
		for (auto item : children) {
			const shared_ptr<const Geometry> &chgeom = item;
			auto chN = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(chgeom);
			if (!chN) {
				if (auto chps = dynamic_pointer_cast<const PolySet>(chgeom))
					chN.reset(createNefPolyhedronFromGeometry(*chps));
			}

			if (chN && !chN->isEmpty()) {
				pointers.push_back(chN);
				CGALUtils::ErrorLocker errorLocker;
				try {
					// nary_union.add_polyhedron() can issue assertion errors:
					// https://github.com/openscad/openscad/issues/802
					nary_union.add_polyhedron(nullptr, *chN);
					error = false;
				}
				// union && difference assert triggered by testdata/scad/bugs/rotate-diff-nonmanifold-crash.scad and testdata/scad/bugs/issue204.scad
				catch (const CGAL::Failure_exception &e) {
					PRINTB("ERROR: CGAL error in CGALUtils::applyUnion: %s", e.what());
					error = true;
				}
				if (error)
					break;
			}
			if (auto progress = CpuProgress::getCurrent())
				progress->tick();
		}

		if (!error)
			return new CGAL_Nef_polyhedron(nary_union.get_union());

		return nullptr;
	}

	/*!
		Applies op to all children and returns the result.
		The child list should be guaranteed to contain non-NULL 3D or empty Geometry objects
	*/
	CGAL_Nef_polyhedron *applyOperator(const GeometryHandles &children, OpenSCADOperator op)
	{
		std::string opstr(OP_NAMES[op]);
		CGAL_Nef_polyhedron *N = NULL;
		if (op == OPENSCAD_UNION)
		{
			// Speeds up n-ary union operations significantly
			if (Feature::ExperimentalThreadedUnion.is_enabled()) {
				Threaded_Nef_nary_union_3 nary_union;
				N = applyUnion(nary_union, children);
			}
			else {
				CGAL_Nef_nary_union_3 nary_union;
				N = applyUnion(nary_union, children);
			}
		}
		else
		{
			LocalProgress progress(opstr, children.size() + 1);
			size_t op_count = 0;
			shared_ptr<const CGAL_Nef_polyhedron> res;
			for (const auto &item : children) {
				const shared_ptr<const Geometry> &chgeom = item;
				shared_ptr<const CGAL_Nef_polyhedron> chN =
					dynamic_pointer_cast<const CGAL_Nef_polyhedron>(chgeom);
				if (!chN) {
					const PolySet *chps = dynamic_cast<const PolySet*>(chgeom.get());
					if (chps) chN.reset(createNefPolyhedronFromGeometry(*chps));
				}
				if (!chN) {
					// ???
					progress.tick();
					continue;
				}

				// Initialize N with first expected geometric object
				if (!res) {
					res = chN;
					progress.tick();
					continue;
				}

				// empty op <something> => empty
				if (res->isEmpty()) {
					PRINTB("empty %s <something> => empty", opstr);
					progress.tick();
					break;
				}

				if (chN->isEmpty()) {
					if (op == OPENSCAD_INTERSECTION) {
						// Intersecting something with nothing results in nothing
						PRINTB("<something> %s empty => empty", opstr);
						res = std::make_shared<CGAL_Nef_polyhedron>();
						progress.tick();
						break;
					}
					// <something> op empty => something
					PRINTB("<something> %s empty => <something>", opstr);
					progress.tick();
					continue;
				}

				CGALUtils::ErrorLocker errorLocker;
				bool error = false;
				try {
					switch (op) {
					case OPENSCAD_INTERSECTION:
						res.reset(res->intersection(*chN));
						break;
					case OPENSCAD_DIFFERENCE:
						res.reset(res->difference(*chN));
						break;
					case OPENSCAD_MINKOWSKI:
						res.reset(res->minkowski(*chN));
						break;
					default:
						PRINTB("ERROR: Unsupported CGAL operator: %d", op);
					}
					progress.tick();
				}
				// union && difference assert triggered by testdata/scad/bugs/rotate-diff-nonmanifold-crash.scad and testdata/scad/bugs/issue204.scad
				catch (const CGAL::Failure_exception &e) {
					PRINTB("ERROR: CGAL error in CGALUtils::applyBinaryOperator %s: %s", opstr % e.what());
					error = true;
				}
				if (error)
					break;
			}
			if (res)
				N = new CGAL_Nef_polyhedron(*res);
		}
		if (auto progress = CpuProgress::getCurrent())
			progress->finish();
		return N;
	}

	void getPoints(const CGAL_Nef_polyhedron &N, std::list<K::Point_3> &points)
	{
		for (CGAL_Nef_polyhedron3::Vertex_const_iterator i = N->vertices_begin(); i != N->vertices_end(); ++i) {
			points.push_back(vector_convert<K::Point_3>(i->point()));
		}
	}

	void getPoints(const PolySet *ps, std::list<K::Point_3> &points)
	{
		for (const auto &p : ps->getPolygons()) {
			for (const auto &v : p) {
				points.push_back(K::Point_3(v[0], v[1], v[2]));
			}
		}
	}

	void getPoints(const shared_ptr<const Geometry> &geom, std::list<K::Point_3> &points)
	{
		if (const CGAL_Nef_polyhedron *N = dynamic_cast<const CGAL_Nef_polyhedron *>(geom.get())) {
			if (!N->isEmpty()) {
				getPoints(*N, points);
			}
		}
		else if (const PolySet *ps = dynamic_cast<const PolySet *>(geom.get())) {
			getPoints(ps, points);
		}
		else if (const GeometryGroup *gg = dynamic_cast<const GeometryGroup *>(geom.get())) {
			for (const auto &child : gg->getChildren())
				getPoints(child.second, points);
		}
	}

	bool applyHull(const GeometryHandles &children, PolySet &result)
	{
		typedef CGAL::Epick K;
		// Collect point cloud
		// NB! CGAL's convex_hull_3() doesn't like std::set iterators, so we use a list
		// instead.
		std::list<K::Point_3> points;

		for (const auto &item : children) {
			getPoints(item, points);
		}

		if (points.size() <= 3) return false;

		// Apply hull
		bool success = false;
		if (points.size() >= 4) {
			CGALUtils::ErrorLocker errorLocker;
			try {
				CGAL::Polyhedron_3<K> r;
				CGAL::convex_hull_3(points.begin(), points.end(), r);
				PRINTDB("After hull vertices: %d", r.size_of_vertices());
				PRINTDB("After hull facets: %d", r.size_of_facets());
				PRINTDB("After hull closed: %d", r.is_closed());
				PRINTDB("After hull valid: %d", r.is_valid());
				success = !createPolySetFromPolyhedron(r, result);
			}
			catch (const CGAL::Failure_exception &e) {
				PRINTB("ERROR: CGAL error in applyHull(): %s", e.what());
			}
		}
		return success;
	}

	class AutoStartTimer : public CGAL::Timer
	{
	public:
		AutoStartTimer() { start(); }
	};


	/*!
		children cannot contain NULL objects
	*/
	Geometry const * applyMinkowski(const GeometryHandles &children)
	{
		CGALUtils::ErrorLocker errorLocker;
		AutoStartTimer t_tot;
		assert(children.size() >= 2);
		try {
			shared_ptr<const Geometry> op0 = children[0];
			for (size_t i = 1; i < children.size(); ++i)
			{
				shared_ptr<const Geometry> op1 = children[i];
				op0 = shared_ptr<const Geometry>(applyMinkowski(op0.get(), op1.get()));
			}
			return op0->copy();
		}
		catch (...) {
			// If anything throws we simply fall back to Nef Minkowski
			PRINTD("Minkowski: Falling back to Nef Minkowski");

			CGAL_Nef_polyhedron *N = applyOperator(children, OPENSCAD_MINKOWSKI);
			return N;
		}
	}

	Geometry const* applyMinkowski(const Geometry *a, const Geometry *b)
	{
		CGALUtils::ErrorLocker errorLocker;
		AutoStartTimer t_tot;
		const Geometry *operands[2] = { a, b };
		try {
			typedef CGAL::Epick Hull_kernel;

			// a and b decomposed into lists of convex polyhedrons
			std::list<CGAL_Polyhedron> P[2];
			for (size_t i = 0; i < 2; i++) {
				AutoStartTimer t;

				CGAL_Polyhedron poly;

				const PolySet * ps = dynamic_cast<const PolySet *>(operands[i]);

				const CGAL_Nef_polyhedron * nef = dynamic_cast<const CGAL_Nef_polyhedron *>(operands[i]);

				if (ps) 
					CGALUtils::createPolyhedronFromPolySet(*ps, poly);
				else if (nef && (*nef)->is_simple()) 
					nefworkaround::convert_to_Polyhedron<CGAL_Kernel3>(**nef, poly);
				else
					throw 0;

				if ((ps && ps->is_convex()) || (!ps && is_weakly_convex(poly))) {
					PRINTDB("Minkowski: child %d is convex %s", i % (ps ? "PolySet" : "Nef"));
					P[i].push_back(poly);
				}
				else {
					CGAL_Nef_polyhedron3 decomposed_nef;

					if (ps) {
						PRINTDB("Minkowski: child %d is nonconvex PolySet, converting to Nef and decomposing...", i);
						CGAL_Nef_polyhedron *p = createNefPolyhedronFromGeometry(*ps);
						if (!p->isEmpty())
							decomposed_nef = **p;
						delete p;
					}
					else {
						PRINTDB("Minkowski: child %d is nonconvex Nef, decomposing...", i);
						decomposed_nef = **nef;
					}

					CGAL::convex_decomposition_3(decomposed_nef);

					// the first volume is the outer volume, which ignored in the decomposition
					CGAL_Nef_polyhedron3::Volume_const_iterator ci = ++decomposed_nef.volumes_begin();
					for (; ci != decomposed_nef.volumes_end(); ++ci) {
						if (ci->mark()) {
							CGAL_Polyhedron poly;
							decomposed_nef.convert_inner_shell_to_polyhedron(ci->shells_begin(), poly);
							P[i].push_back(poly);
						}
					}


					PRINTDB("Minkowski: decomposed into %d convex parts", P[i].size());
					PRINTDB("Minkowski: decomposition took %f s", t.time());
				}
			}

			// sums of convex parts
			std::list<CGAL::Polyhedron_3<Hull_kernel>> result_parts;
			for (size_t i = 0; i < P[0].size(); i++) {
				for (size_t j = 0; j < P[1].size(); j++) {
					std::vector<Hull_kernel::Point_3> minkowski_points;
					{
						AutoStartTimer t;
						std::vector<Hull_kernel::Point_3> points[2];

						for (int k = 0; k < 2; k++) {
							std::list<CGAL_Polyhedron>::iterator it = P[k].begin();
							std::advance(it, k == 0 ? i : j);

							CGAL_Polyhedron const& poly = *it;
							points[k].reserve(poly.size_of_vertices());

							for (CGAL_Polyhedron::Vertex_const_iterator pi = poly.vertices_begin(); pi != poly.vertices_end(); ++pi) {
								CGAL_Polyhedron::Point_3 const& p = pi->point();
								points[k].push_back(Hull_kernel::Point_3(to_double(p[0]), to_double(p[1]), to_double(p[2])));
							}
						}

						minkowski_points.clear();
						minkowski_points.reserve(points[0].size() * points[1].size());
						for (size_t i = 0; i < points[0].size(); i++) {
							for (size_t j = 0; j < points[1].size(); j++) {
								minkowski_points.push_back(points[0][i] + (points[1][j] - CGAL::ORIGIN));
							}
						}

						if (minkowski_points.size() <= 3) {
							continue;
						}

						PRINTDB("Minkowski: Point cloud creation (%d ⨉ %d -> %d) took %f ms", points[0].size() % points[1].size() % minkowski_points.size() % (t.time() * 1000));
					}

					{
						AutoStartTimer t;
						CGAL::Polyhedron_3<Hull_kernel> result;
						CGAL::convex_hull_3(minkowski_points.begin(), minkowski_points.end(), result);

						std::vector<Hull_kernel::Point_3> strict_points;
						strict_points.reserve(minkowski_points.size());

						for (CGAL::Polyhedron_3<Hull_kernel>::Vertex_iterator i = result.vertices_begin(); i != result.vertices_end(); ++i) {
							Hull_kernel::Point_3 const& p = i->point();

							CGAL::Polyhedron_3<Hull_kernel>::Vertex::Halfedge_handle h, e;
							h = i->halfedge();
							e = h;
							bool collinear = false;
							bool coplanar = true;

							do {
								Hull_kernel::Point_3 const& q = h->opposite()->vertex()->point();
								if (coplanar && !CGAL::coplanar(p, q,
									h->next_on_vertex()->opposite()->vertex()->point(),
									h->next_on_vertex()->next_on_vertex()->opposite()->vertex()->point())) {
									coplanar = false;
								}


								for (CGAL::Polyhedron_3<Hull_kernel>::Vertex::Halfedge_handle j = h->next_on_vertex();
									j != h && !collinear && !coplanar;
									j = j->next_on_vertex()) {

									Hull_kernel::Point_3 const& r = j->opposite()->vertex()->point();
									if (CGAL::collinear(p, q, r)) {
										collinear = true;
									}
								}

								h = h->next_on_vertex();
							} while (h != e && !collinear);

							if (!collinear && !coplanar)
								strict_points.push_back(p);
						}

						result.clear();
						CGAL::convex_hull_3(strict_points.begin(), strict_points.end(), result);
						result_parts.push_back(result);

						PRINTDB("Minkowski: Computing convex hull took %f s", t.time());
					}
				}
			}

			if (result_parts.size() == 1) {
				PolySet *ps = new PolySet(3, true);
				createPolySetFromPolyhedron(*result_parts.begin(), *ps);
				operands[0] = ps;
			}
			else if (!result_parts.empty()) {
				AutoStartTimer t;
				PRINTDB("Minkowski: Computing union of %d parts", result_parts.size());
				GeometryHandles fake_children;
				for (std::list<CGAL::Polyhedron_3<Hull_kernel>>::iterator i = result_parts.begin(); i != result_parts.end(); ++i) {
					PolySet ps(3, true);
					createPolySetFromPolyhedron(*i, ps);
					fake_children.push_back(GeometryHandle(createNefPolyhedronFromGeometry(ps)));
				}
				CGAL_Nef_polyhedron *N = CGALUtils::applyOperator(fake_children, OPENSCAD_UNION);
				// FIXME: This hould really never throw.
				// Assert once we figured out what went wrong with issue #1069?
				if (!N) throw 0;
				PRINTDB("Minkowski: Union done: %f s", t.time());
				operands[0] = N;
			}
			else {
				operands[0] = new CGAL_Nef_polyhedron();
			}

			PRINTDB("Minkowski: Total execution time %f s", t_tot.time());
			return operands[0];
		}
		catch (...) {
			// If anything throws we simply fall back to Nef Minkowski
			PRINTD("Minkowski: Falling back to Nef Minkowski");
			GeometryHandles geom = { GeometryHandle(a->copy()), GeometryHandle(b->copy()) };
			CGAL_Nef_polyhedron *N = applyOperator(geom, OPENSCAD_MINKOWSKI);
			return N;
		}
	}

}; // namespace CGALUtils


#endif // ENABLE_CGAL

