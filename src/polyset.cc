/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "polyset.h"
#include "polyset-utils.h"
#include "linalg.h"
#include "printutils.h"
#include "grid.h"
#include <Eigen/LU>

#include "cgal.h"
#include "cgalutils.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/connected_components.h>
#include <CGAL/boost/graph/Face_filtered_graph.h>
#include <boost/property_map/property_map.hpp>

/*! /class PolySet

	The PolySet class fulfils multiple tasks, partially for historical reasons.
	FIXME: It's a bit messy and is a prime target for refactoring.

	1) Store 2D and 3D polygon meshes from all origins
	2) Store 2D outlines, used for rendering edges (2D only)
	3) Rendering of polygons and edges


	PolySet must only contain convex polygons

 */

PolySet::PolySet(const PolySet &ps)
	: polygons(ps.polygons), polygon(ps.polygon), dim(ps.dim), convex(ps.convex), bbox(ps.bbox), polyDim(ps.polyDim)
{
	type = "PolySet from PolySet";
	data.polySet = this;
}

PolySet::PolySet(unsigned int dim, boost::tribool convex)
	: dim(dim), convex(convex), polyDim(0)
{
	type = "PolySet";
	data.polySet = this;
}

PolySet::PolySet(const Polygon2d &origin) 
	: polygon((Polygon2d*)origin.copy()), dim(2), convex(unknown), polyDim(0)
{
	type = "PolySet from Polygon";
	data = polygon->data; // copy polygon's data so poly/skele is preserved
	data.polySet = this;  // also preserve "this"
}

PolySet::~PolySet()
{
	resetDisplayLists();
	polygon.reset(); // not needed?
}

void PolySet::resetDisplayLists()
{
	for (int i = 0; i <= DisplayLists::MAX_DISPLAY_LISTS; ++i) {
		if (displayLists[i] != 0) {
			glDeleteLists(displayLists[i], 1);
			displayLists[i] = 0;
		}
	}
}

std::string PolySet::dump() const
{
	std::stringstream out;
	out << "PolySet:"
		<< "\n dimensions:" << this->dim
		<< "\n convexity:" << this->convexity
		<< "\n num polygons: " << polygons.size();
	out << "\n polygons data:";
	for (size_t i = 0; i < polygons.size(); i++) {
		out << "\n  polygon begin:";
		const Polygon *poly = &polygons[i];
		for (size_t j = 0; j < poly->size(); j++) {
			Vector3d v = poly->at(j);
			out << "\n   vertex:" << v.transpose();
		}
	}
	if (polygon) {
		out << "\n num outlines: " << polygon->outlines().size();
		out << "\n outlines data:";
		out << polygon->dump();
	}
	out << "\nPolySet end";
	return out.str();
}

void PolySet::append_poly()
{
	polygons.push_back(Polygon());
}

void PolySet::append_poly(const Polygon &poly)
{
	polygons.push_back(poly);
	this->polyDim = std::max(this->polyDim, polygons.back().size());
	for (const auto &v : poly)
		bbox.extend(v);
	this->resetDisplayLists();
}

void PolySet::append_vertex(double x, double y, double z)
{
	append_vertex(Vector3d(x, y, z));
}

void PolySet::append_vertex(const Vector3d &v)
{
	polygons.back().push_back(v);
	this->polyDim = std::max(this->polyDim, polygons.back().size());
	bbox.extend(v);
	this->resetDisplayLists();
}

void PolySet::append_vertex(const Vector3f &v)
{
	append_vertex((const Vector3d &)v.cast<double>());
}

void PolySet::insert_vertex(double x, double y, double z)
{
	insert_vertex(Vector3d(x, y, z));
}

void PolySet::insert_vertex(const Vector3d &v)
{
	polygons.back().insert(polygons.back().begin(), v);
	this->polyDim = std::max(this->polyDim, polygons.back().size());
	bbox.extend(v);
	this->resetDisplayLists();
}

void PolySet::insert_vertex(const Vector3f &v)
{
	insert_vertex((const Vector3d &)v.cast<double>());
}

BoundingBox PolySet::getBoundingBox() const
{
	return this->bbox;
}

size_t PolySet::memsize() const
{
	size_t mem = 0;
	for(const auto &p : this->polygons) 
		mem += p.size() * sizeof(Vector3d);
	if (polygon)
		mem += this->polygon->memsize() - sizeof(*this->polygon);
	mem += sizeof(PolySet);
	return mem;
}

void PolySet::append(const PolySet &ps)
{
	this->polygons.insert(this->polygons.end(), ps.polygons.begin(), ps.polygons.end());
	this->polyDim = std::max(this->polyDim, ps.polyDim);
	this->bbox.extend(ps.getBoundingBox());
	this->resetDisplayLists();
}

void PolySet::translate(const Vector3d &translation)
{
	this->bbox.setNull();
	for (auto &p : polygons) {
		for (auto &v : p) {
			v += translation;
			this->bbox.extend(v);
		}
	}
}

void PolySet::transform(const Transform3d &mat)
{
	// If mirroring transform, flip faces to avoid the object to end up being inside-out
	bool mirrored = mat.matrix().determinant() < 0;

	this->bbox.setNull();
	for(auto &p : this->polygons){
		for(auto &v : p) {
			v = mat * v;
			this->bbox.extend(v);
		}
		if (mirrored) std::reverse(p.begin(), p.end());
	}
	this->resetDisplayLists();
}

bool PolySet::is_convex() const {
	if (convex || this->isEmpty()) return true;
	if (!convex) return false;
	return PolysetUtils::is_approximately_convex(*this);
}

void PolySet::resize(const Vector3d &newsize, const Eigen::Matrix<bool,3,1> &autosize)
{
	BoundingBox bbox = this->getBoundingBox();

  // Find largest dimension
	int maxdim = 0;
	for (int i=1;i<3;i++) if (newsize[i] > newsize[maxdim]) maxdim = i;

	// Default scale (scale with 1 if the new size is 0)
	Vector3d scale(1,1,1);
	for (int i=0;i<3;i++) if (newsize[i] > 0) scale[i] = newsize[i] / bbox.sizes()[i];

  // Autoscale where applicable 
	double autoscale = scale[maxdim];
	Vector3d newscale;
	for (int i=0;i<3;i++) newscale[i] = (!autosize[i] || (newsize[i] > 0)) ? scale[i] : autoscale;
	
	Transform3d t;
	t.matrix() << 
    newscale[0], 0, 0, 0,
    0, newscale[1], 0, 0,
    0, 0, newscale[2], 0,
    0, 0, 0, 1;

	this->transform(t);
}

/*!
	Quantizes vertices by gridding them as well as merges close vertices belonging to
	neighboring grids.
	May reduce the number of polygons if polygons collapse into < 3 vertices.
*/
void QuantizedPolySet::quantizeVertices()
{
	int numverts = 0;
	for (std::vector<Polygon>::iterator iter = this->polygons.begin(); iter != this->polygons.end(); ++iter)
		numverts += iter->size();
	int removeCount = 0;
	PRINTB("Quantize PolySet: %d vertices, %d faces, res=%f", numverts % this->polygons.size() % grid.res);
	for (std::vector<Polygon>::iterator iter = this->polygons.begin(); iter != this->polygons.end(); ) {
		Polygon &p = *iter;
		// Quantize all vertices. Build index list
		std::vector<int> indices(p.size());
		for (int i = 0; i < p.size(); i++) {
			indices[i] = grid.align(p[i]);
		}
		// Remove consequtive duplicate vertices
		Polygon::iterator currp = p.begin();
		for (int i = 0; i < indices.size(); i++) {
			if (i == 0 || indices[i - 1] != indices[i]) {
				(*currp++) = p[i];
			}
		}
		// Erase unused vertices
		if (currp != p.end()) {
			p.erase(currp, p.end());
			numverts -= (p.end() - currp);
		}
		// update the iterator
		if ((p.size() < 3 && !p.open) || p.size() < 2) {
			removeCount++;
			iter = this->polygons.erase(iter);
		}
		else {
			iter++;
		}
	}
	PRINTB("Quantize result: %d vertices, %d faces (removed %d)", numverts % this->polygons.size() % removeCount);
}

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_3 Point;
typedef CGAL::Surface_mesh<Point> Mesh;
typedef boost::graph_traits<Mesh>::face_descriptor face_descriptor;
typedef boost::graph_traits<Mesh>::faces_size_type faces_size_type;
typedef Mesh::Property_map<face_descriptor, faces_size_type> FCCmap;
typedef CGAL::Face_filtered_graph<Mesh> Filtered_graph;

namespace PMP = CGAL::Polygon_mesh_processing;

bool PolySetValidator::validate(const PolySet &ps)
{
	CGALUtils::ErrorLocker errorLocker;

	QuantizedPolySet qps(ps);
	auto qgrid = qps.getGrid();

	const auto *psv = qgrid.getArray();
	size_t psvc = qgrid.db.size();

	PolySet tps(qps);
	if (tps.poly_dim() != 3) {
		PRINTB("Tesselating %d faces (poly_dim=%d)", tps.getPolygons().size() % tps.poly_dim());
		PolysetUtils::tessellate_faces(tps, tps);
	}

	PRINTB("Building mesh: adding %d vertices", psvc);
	Mesh mesh;
	for (size_t i = 0; i < psvc; ++i)
		mesh.add_vertex(Point(psv[i][0], psv[i][1], psv[i][2]));

	PRINTB("Building mesh: adding %d faces", tps.getPolygons().size());
	for (const auto &p : tps.getPolygons()) {
		std::vector<Mesh::Vertex_index> pp;
		for (const auto &v : p) {
			pp.push_back(Mesh::Vertex_index(qgrid.data(v)));
		}
		mesh.add_face(pp);
	}
	
	bool closed = CGAL::is_closed(mesh);
	PRINTB("Mesh is %s", (closed ? "closed" : "open"));

	FCCmap fccmap = mesh.add_property_map<face_descriptor, faces_size_type>("f:CC").first;
	faces_size_type num = PMP::connected_components(mesh, fccmap);
	PRINTB("- The graph has %d connected components (face connectivity)", num);

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

	bool intersecting = PMP::does_self_intersect(mesh,
		PMP::parameters::vertex_point_map(get(CGAL::vertex_point, mesh)));
	PRINT(intersecting ? "WARNING: There are self-intersections." : "There is no self-intersection.");
	if (false && intersecting) {
		std::vector<std::pair<face_descriptor, face_descriptor> > intersected_tris;
		PMP::self_intersections(mesh, std::back_inserter(intersected_tris));
		PRINTB("WARNING: %d pairs of triangles intersect. %d total.", intersected_tris.size() % tps.getPolygons().size());
		if (false) {
			for (const auto &tri : intersected_tris) {
				for (size_t i : mesh.vertices_around_face(CGAL::halfedge(tri.first, mesh))) {

				}
			}
		}
	}

	return intersecting;
}
