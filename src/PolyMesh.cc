#include "PolyMesh.h"
#include "polyset.h"
#include "polyset-utils.h"
#include "linalg.h"
#include "printutils.h"
#include "grid.h"
#include <Eigen/LU>

#include <vector>
#include <string>
#include <sstream>

#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/connected_components.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>

namespace PMP = CGAL::Polygon_mesh_processing;


PolyMesh::PolyMesh(const PolyMesh::Mesh &mesh, const std::string &name)
	: QuantizedPolySet(PolySet(3))
	, name(name)
{
	auto pm = get(CGAL::vertex_point, mesh);
	for (const auto &f : mesh.faces()) {
		append_poly();
		auto edg = mesh.halfedge(f);
		auto edgb = edg;
		do {
			auto v = pm[mesh.target(edg)];
			append_vertex(Vector3d(v[0], v[1], v[2]));
			edg = mesh.next(edg);
		} while (edg != edgb);
	}

	finishCreate();
}

PolyMesh::PolyMesh(const PolySet &ps, const std::string &name)
	: QuantizedPolySet(ps)
	, name(name)
{
	finishCreate();
}

template <typename face_descriptor, typename PM>
bool triangulateFace(face_descriptor f, PM& pmesh)
{
	typedef PolyMesh::Kernel::FT FT;
	typedef PolyMesh::Kernel::Point_3 Point_ref;
	typedef PolyMesh::halfedge_descriptor halfedge_descriptor;

	auto vpmap = CGAL::get(CGAL::vertex_point, pmesh);

	std::size_t original_size = CGAL::halfedges_around_face(halfedge(f, pmesh), pmesh).size();
	if (original_size == 4)
	{
		halfedge_descriptor v0, v1, v2, v3;
		v0 = halfedge(f, pmesh);
		Point_ref p0 = vpmap[target(v0, pmesh)];
		v1 = next(v0, pmesh);
		Point_ref p1 = vpmap[target(v1, pmesh)];
		v2 = next(v1, pmesh);
		Point_ref p2 = vpmap[target(v2, pmesh)];
		v3 = next(v2, pmesh);
		Point_ref p3 = vpmap[target(v3, pmesh)];

		/* Chooses the diagonal that will split the quad in two triangles that maximize
		 * the scalar product of of the un-normalized normals of the two triangles.
		 * The lengths of the un-normalized normals (computed using cross-products of two vectors)
		 *  are proportional to the area of the triangles.
		 * Maximize the scalar product of the two normals will avoid skinny triangles,
		 * and will also taken into account the cosine of the angle between the two normals.
		 * In particular, if the two triangles are oriented in different directions,
		 * the scalar product will be negative.
		 */
		FT p1p3 = CGAL::cross_product(p2 - p1, p3 - p2) * CGAL::cross_product(p0 - p3, p1 - p0);
		FT p0p2 = CGAL::cross_product(p1 - p0, p1 - p2) * CGAL::cross_product(p3 - p2, p3 - p0);
		if (p0p2 > p1p3)
		{
			CGAL::Euler::split_face(v0, v2, pmesh);
		}
		else
		{
			CGAL::Euler::split_face(v1, v3, pmesh);
		}
		return true;
	}
	return PMP::triangulate_face(f, pmesh);
}

void PolyMesh::finishCreate()
{
	CGALUtils::ErrorLocker errorLocker;

	const auto *psv = grid.getArray();
	size_t psvc = grid.db.size();

	if (false && poly_dim() != 3) {
		PRINTB("Tesselating %d faces (poly_dim=%d)", polygons.size() % poly_dim());
		PolysetUtils::tessellate_faces(*this, *this);
	}

	PRINTB("Building mesh: adding %d vertices", psvc);
	for (size_t i = 0; i < psvc; ++i)
		mesh.add_vertex(Point(psv[i][0], psv[i][1], psv[i][2]));

	PRINTB("Building mesh: adding %d faces", polygons.size());
	for (const auto &p : polygons) {
		std::vector<Mesh::Vertex_index> pp;
		for (const auto &v : p) {
			pp.push_back(Mesh::Vertex_index(grid.data(v)));
		}
		auto fi = mesh.add_face(pp);
		if (!CGAL::is_triangle(mesh.halfedge(fi), mesh)) {
			bool res = triangulateFace(fi, mesh);
			PRINTB("....Triangulation: %s", (res ? "success" : "FAIL!!!"));
		}
	}

	fccmap = mesh.add_property_map<face_descriptor, faces_size_type>("f:CC").first;
}

PolyMesh::~PolyMesh()
{
}

bool PolyMesh::validate()
{
	CGALUtils::ErrorLocker errorLocker;

	bool closed = CGAL::is_closed(mesh);
	bool isTri = CGAL::is_triangle_mesh(mesh);
	PRINT("Mesh validation:");
	PRINTB("    %s", (closed ? "Closed" : "Open"));
	PRINTB("    %s", (isTri ? "Triangles" : "Not triangles"));

	if (!isTri) {
		PRINT("....Triangulating mesh");
		bool res = PMP::triangulate_faces(mesh);
		PRINTB("....%s", (res ? "Success" : "FAIL!!!"));
		if (!res) {
			int i = 0;
			bool done;
			do {
				done = true;
				for (auto f : mesh.faces()) {
					if (!CGAL::is_triangle(mesh.halfedge(f), mesh)) {
						res = PMP::triangulate_face(f, mesh);
						if (res) {
							PRINTB("....face %d: %s", (++i) % (res ? "Success" : "FAIL!!!"));
						}
						done = false;
						break;
					}
				}
			} while (!done);
		}
	}

	faces_size_type num = PMP::connected_components(mesh, fccmap);
	PRINTB("	%d connected components (face connectivity)", num);

	if (false) {
		for (size_t i = 0; i < num; ++i) {
			Filtered_graph ffg(mesh, i, fccmap);
			PRINTB("CC #%d:", (i + 1));
			if (!ffg.is_selection_valid()) {
				PRINT("Invalid selection");
				continue;
			}
			bool ffc = CGAL::is_closed(ffg);
			PRINTB("    Vertices: %d", ffg.number_of_vertices());
			PRINTB("    Faces: %d", ffg.number_of_faces());
			PRINTB("    %s", (ffc ? "Closed" : "NOT closed"));
			if (ffc) {
				try {
					if (PMP::is_outward_oriented(ffg,
						PMP::parameters::vertex_point_map(get(CGAL::vertex_point, ffg)))) {
						PRINT("    Outward oriented");
						/*
						for (boost::graph_traits<Filtered_graph>::face_descriptor f : faces(ffg)) {
							for (size_t i : mesh.vertices_around_face(CGAL::halfedge(f, mesh))) {
							}
						}
						*/
					}
					else
						PRINT("    NOT outward oriented");
				}
				catch (const CGAL::Assertion_exception &e) {
					PRINTB("Warning: Cannot detect orientation of component: %s", e.what());
				}
			}
		}
	}

	if (CGAL::is_triangle_mesh(mesh)) {
		bool intersecting = PMP::does_self_intersect(mesh,
			PMP::parameters::vertex_point_map(get(CGAL::vertex_point, mesh)));
		PRINT(intersecting ? "WARNING: There are self-intersections." : "    No self-intersection");
		if (intersecting) {
			std::vector<std::pair<face_descriptor, face_descriptor> > intersected_tris;
			PMP::self_intersections(mesh, std::back_inserter(intersected_tris));
			PRINTB("    %d intersecting pairs (%d total)", intersected_tris.size() % polygons.size());
			if (true) {
				const auto &pm = get(CGAL::vertex_point, mesh);
				int i = 0;
				for (const auto &tri : intersected_tris) {
					if (i > 5) {
						PRINTB("  ... (+ %d more)", (intersected_tris.size() - 5));
						break;
					}
					std::stringstream str;
					auto edg = mesh.halfedge(tri.first);
					auto edgb = edg;
					str << "    Intersection " << (++i) << ": [";
					do
					{
						auto vi = mesh.target(edg);
						const auto &p = pm[vi];
						if (edg != edgb)
							str << ", ";
						str << "[" << p << " (" << vi << ")" << "]";
						edg = mesh.next(edg);
					} while (edg != edgb);
					str << "]/[";
					edg = mesh.halfedge(tri.second);
					edgb = edg;
					do
					{
						auto vi = mesh.target(edg);
						const auto &p = pm[vi];
						if (edg != edgb)
							str << ", ";
						str << "[" << p << " (" << vi << ")" << "]";
						edg = mesh.next(edg);
					} while (edg != edgb);
					str << "]";
					PRINT(str.str());
				}
			}
		}
	}

	return true;
}