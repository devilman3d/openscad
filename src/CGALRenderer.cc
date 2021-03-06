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

#ifdef _MSC_VER 
 // Boost conflicts with MPFR under MSVC (google it)
#include <mpfr.h>
#endif

// dxfdata.h must come first for Eigen SIMD alignment issues
#include "dxfdata.h"
#include "polyset.h"
#include "polyset-utils.h"
#include "printutils.h"

#include "CGALRenderer.h"
#include "CGAL_OGL_Polyhedron.h"
#include "CGAL_Nef_polyhedron.h"
#include "cgal.h"
#include "Polygon2d-CGAL.h"

//#include "Preferences.h"

CGALRenderer::CGALRenderer(shared_ptr<const class Geometry> geom)
{
	addGeometry(geom);
}

CGALRenderer::~CGALRenderer()
{
}

void CGALRenderer::setGeometry(shared_ptr<const class Geometry> geom)
{
	polysets.clear();
	nefs.clear();
	polyhedrons.clear();
	addGeometry(geom);
}

void CGALRenderer::addGeometry(shared_ptr<const class Geometry> geom)
{
	if (shared_ptr<const PolySet> ps = dynamic_pointer_cast<const PolySet>(geom)) {
		assert(ps->getDimension() == 3);
		//if (false)
		{
			// We need to tessellate here, in case the generated PolySet contains concave polygons
			// See testdata/scad/3D/features/polyhedron-concave-test.scad
			PolySet *ps_tri = new PolySet(3, ps->convexValue());
			ps_tri->setConvexity(ps->getConvexity());
			PolysetUtils::tessellate_faces(*ps, *ps_tri);
			polysets.push_back(shared_ptr<const PolySet>(ps_tri));
		}
		//else {
		//	polysets.push_back(ps);
		//}
	}
	else if (shared_ptr<const Polygon2d> poly = dynamic_pointer_cast<const Polygon2d>(geom)) {
		polysets.push_back(shared_ptr<const PolySet>(poly->tessellate()));
	}
	else if (shared_ptr<const CGAL_Nef_polyhedron> nef = dynamic_pointer_cast<const CGAL_Nef_polyhedron>(geom)) {
		assert(nef->getDimension() == 3);
		if (!nef->isEmpty()) {
			this->nefs.push_back(nef);
			buildCGALPolyhedron(this->colorscheme, nef);
		}
	}
	else if (shared_ptr<const GeometryGroup> gg = dynamic_pointer_cast<const GeometryGroup>(geom)) {
		if (!gg->isEmpty()) {
			for (const auto &child : gg->getChildren()) {
				addGeometry(child.second);
			}
		}
	}
}

void CGALRenderer::buildCGALPolyhedron(const ColorScheme *scheme, const shared_ptr<const CGAL_Nef_polyhedron> &geom)
{
	auto polyhedron = new CGAL_OGL_Polyhedron(*scheme);
	CGAL::OGL::Nef3_Converter<CGAL_Nef_polyhedron3>::convert_to_OGLPolyhedron(**geom, polyhedron);
	// CGAL_NEF3_MARKED_FACET_COLOR <- CGAL_FACE_BACK_COLOR
	// CGAL_NEF3_UNMARKED_FACET_COLOR <- CGAL_FACE_FRONT_COLOR
	polyhedron->init();
	polyhedrons.push_back(std::shared_ptr<CGAL_OGL_Polyhedron>(polyhedron));
}

// Overridden from Renderer
void CGALRenderer::setColorScheme(const ColorScheme &cs)
{
	PRINTD("setColorScheme");
	Renderer::setColorScheme(cs);
	this->polyhedrons.clear();
	for (auto &nef : this->nefs)
		buildCGALPolyhedron(this->colorscheme, nef);
	PRINTD("setColorScheme done");
}

void CGALRenderer::draw(bool showfaces, bool showedges) const
{
	PRINTD("draw()");
	if (!this->polysets.empty()) {
		for (auto ps : polysets) {
			PRINTD("draw() polyset");
			if (ps->getDimension() == 2) {
				// Draw 2D polygons
				glDisable(GL_LIGHTING);
				// FIXME:		const QColor &col = Preferences::inst()->color(Preferences::CGAL_FACE_2D_COLOR);
				glColor3f(0.0f, 0.75f, 0.60f);

				for (const auto &poly : ps->getPolygons()) {
					glBegin(poly.open ? GL_LINE_STRIP : GL_POLYGON);
					for (size_t j = 0; j < poly.size(); j++) {
						const Vector3d &p = poly[j];
						glVertex3d(p[0], p[1], 0);
					}
					glEnd();
				}

				// Draw 2D edges
				glDisable(GL_DEPTH_TEST);

				glLineWidth(2);
				// FIXME:		const QColor &col2 = Preferences::inst()->color(Preferences::CGAL_EDGE_2D_COLOR);
				glColor3f(1.0f, 0.0f, 0.0f);
				ps->render_edges(CSGMODE_NONE);

				glEnable(GL_DEPTH_TEST);
			}
			else {
				// Draw 3D polygons
				glEnable(GL_LIGHTING);
				const Color4f c(-1, -1, -1, -1);
				setColor(COLORMODE_MATERIAL, c.data());
				if (showfaces)
					ps->render_surface(CSGMODE_NORMAL, false);
				if (showedges)
					ps->render_edges(CSGMODE_NORMAL);
			}
		}
	}
	if (!this->polyhedrons.empty()) {
		for (auto polyhedron : polyhedrons) {
			PRINTD("draw() polyhedron");
			polyhedron->draw(showfaces, showedges);
		}
	}
	PRINTD("draw() end");
}

BoundingBox CGALRenderer::getBoundingBox() const
{
	BoundingBox bbox;
	for (auto ps : polysets) {
		auto temp = ps->getBoundingBox();
		bbox.extend(temp);
	}
	for (auto polyhedron : nefs) {
		auto temp = polyhedron->getBoundingBox();
		bbox.extend(temp);
	}
	return bbox;
}
