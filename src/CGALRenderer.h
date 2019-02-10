#pragma once

#include "renderer.h"
#include "CGAL_Nef_polyhedron.h"

class CGAL_OGL_Polyhedron;
class Geometry;
class PolySet;

class CGALRenderer : public Renderer
{
public:
	CGALRenderer(shared_ptr<const Geometry> geom);
	~CGALRenderer();
	virtual void draw(bool showfaces, bool showedges) const;
	virtual void setColorScheme(const ColorScheme &cs);
	virtual BoundingBox getBoundingBox() const;

	void addGeometry(shared_ptr<const Geometry> geom);
	void setGeometry(shared_ptr<const Geometry> geom);

	shared_ptr<const class Geometry> getGeometry() const { return geom; }

private:
	void buildCGALPolyhedron(const ColorScheme *scheme, const shared_ptr<const CGAL_Nef_polyhedron> &geom);

	std::vector<shared_ptr<const CGAL_OGL_Polyhedron>> polyhedrons;
	std::vector<shared_ptr<const CGAL_Nef_polyhedron>> nefs;
	std::vector<shared_ptr<const PolySet>> polysets;
	shared_ptr<const Geometry> geom;
};
