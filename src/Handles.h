#pragma once

#include <vector>
#include "memory.h"
#include "maybe_const.h"

class AbstractNode;
class Geometry;
class GeometryGroup;
class Polygon2d;
class PolySet;
class CGAL_Nef_polyhedron;
class RenderGeometry;

using std::pair;
using std::vector;

typedef shared_ptr<AbstractNode> NodeHandle;
typedef shared_ptr<const AbstractNode> ConstNodeHandle;

typedef shared_ptr<const Geometry> GeometryHandle;

typedef shared_ptr<const GeometryGroup> GeometryGroupHandle;

typedef shared_ptr<const Polygon2d> Polygon2dHandle;

typedef shared_ptr<PolySet> PolySetHandle;
typedef shared_ptr<const PolySet> ConstPolySetHandle;

typedef shared_ptr<CGAL_Nef_polyhedron> NefHandle;
typedef shared_ptr<const CGAL_Nef_polyhedron> ConstNefHandle;

typedef shared_ptr<const RenderGeometry> RenderHandle;

typedef pair<const AbstractNode*, GeometryHandle> NodeGeometry;
typedef vector<NodeGeometry> NodeGeometries;

typedef pair<const AbstractNode*, RenderHandle> RenderNodeGeometry;
typedef vector<RenderNodeGeometry> RenderNodeGeometries;

typedef vector<NodeHandle> NodeHandles;
typedef vector<ConstNodeHandle> ConstNodeHandles;
typedef vector<GeometryHandle> GeometryHandles;
typedef vector<Polygon2dHandle> Polygon2dHandles;
typedef vector<RenderHandle> RenderHandles;

typedef vector<const Geometry *> Geometries;
typedef vector<const Polygon2d *> Polygon2ds;

typedef maybe_const<Geometry> ResultObject;
