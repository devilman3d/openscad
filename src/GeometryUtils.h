#pragma once

#include "linalg.h"
#include <vector>

class Polygon : public std::vector<Vector3d>
{
public:
	Polygon() : open(false) { }
	bool open;
};

typedef std::vector<Polygon> Polygons;

typedef std::vector<int> IndexedFace;
typedef Vector3i IndexedTriangle;

struct IndexedPolygons {
	std::vector<Vector3f> vertices;
	std::vector<IndexedFace> faces;
};

struct IndexedTriangleMesh {
	std::vector<Vector3f> vertices;
	std::vector<IndexedTriangle> triangles;
};

// Indexed polygon mesh, where each polygon can have holes
struct IndexedPolyMesh {
	std::vector<Vector3f> vertices;
	std::vector<std::vector<IndexedFace>> polygons;
};

namespace GeometryUtils {
	bool tessellatePolygon(const Polygon &polygon,
						   Polygons &triangles,
						   const Vector3f *normal = NULL);
	bool tessellatePolygonWithHoles(const Vector3f *vertices,
									const std::vector<IndexedFace> &faces, 
									std::vector<IndexedTriangle> &triangles,
									const Vector3f *normal = NULL);

	int findUnconnectedEdges(const std::vector<std::vector<IndexedFace>> &polygons);
	int findUnconnectedEdges(const std::vector<IndexedTriangle> &triangles);
}
