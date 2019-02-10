#pragma once

#include <string>

#include "polyset.h"

#include "cgal.h"
#include "cgalutils.h"

#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/Face_filtered_graph.h>
#include <boost/property_map/property_map.hpp>

class PolyMesh : public QuantizedPolySet
{
public:
	typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
	//typedef CGAL_Kernel3 Kernel;
	typedef Kernel::Point_3 Point;
	typedef CGAL::Surface_mesh<Point> Mesh;
	typedef boost::graph_traits<Mesh>::halfedge_descriptor halfedge_descriptor;
	typedef boost::graph_traits<Mesh>::face_descriptor face_descriptor;
	typedef boost::graph_traits<Mesh>::faces_size_type faces_size_type;
	typedef Mesh::Property_map<face_descriptor, faces_size_type> FCCmap;
	typedef CGAL::Face_filtered_graph<Mesh> Filtered_graph;

private:
	//PolyMesh(const PolyMesh &) delete;
	void finishCreate();

public:
	PolyMesh(const Mesh &mesh, const std::string &name = std::string());
	PolyMesh(const PolySet &ps, const std::string &name = std::string());
	virtual ~PolyMesh();

	bool validate();

	const Mesh &getMesh() const { return mesh; }
	Mesh &getMesh() { return mesh; }

private:
	Mesh mesh;
	FCCmap fccmap;
	std::string name;
};
