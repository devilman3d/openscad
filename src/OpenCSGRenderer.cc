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

#include "system-gl.h"
#include "OpenCSGRenderer.h"
#include "polyset.h"
#include "csgnode.h"
#include "stl-utils.h"

#ifdef ENABLE_OPENCSG
#  include <opencsg.h>

class OpenCSGPrim : public OpenCSG::Primitive
{
public:
	OpenCSGPrim(const CSGChainObject &csgobj, OpenCSG::Operation operation)
		: OpenCSG::Primitive(operation, csgobj.leaf->geom->getConvexity())
		, geom(csgobj.leaf->geom)
		, m(csgobj.leaf->matrix)
	{
	}

	shared_ptr<const Geometry> geom;
	Transform3d m;

	virtual void render() {
		glPushMatrix();
		glMultMatrixd(m.data());
		Renderer::render_surface(geom, Renderer::CSGMODE_NONE, m.matrix().determinant() < 0);
		glPopMatrix();
	}
};

#endif

OpenCSGRenderer::OpenCSGRenderer(shared_ptr<CSGProducts> root_products,
								 shared_ptr<CSGProducts> highlights_products,
								 shared_ptr<CSGProducts> background_products)
	: root_products(root_products)
	, highlights_products(highlights_products)
	, background_products(background_products)
{
}

void OpenCSGRenderer::draw(bool /*showfaces*/, bool showedges) const
{
	if (this->root_products) {
		renderCSGProducts(*this->root_products, showedges, false, false);
	}
	if (this->background_products) {
		renderCSGProducts(*this->background_products, showedges, false, true);
	}
	if (this->highlights_products) {
		renderCSGProducts(*this->highlights_products, showedges, true, false);
	}
}

void OpenCSGRenderer::renderCSGProducts(const CSGProducts &products, bool showedges, bool highlight_mode, bool background_mode) const
{
#ifdef ENABLE_OPENCSG
	std::vector<shared_ptr<OpenCSGPrim>> handles;
	for(const auto &product : products.products) {
		std::vector<OpenCSG::Primitive*> primitives;
		for(const auto &csgobj : product.intersections) {
			if (csgobj.leaf->geom) {
				auto prim = std::make_shared<OpenCSGPrim>(csgobj, OpenCSG::Intersection);
				handles.push_back(prim);
				primitives.push_back(prim.get());
			}
		}
		for(const auto &csgobj : product.subtractions) {
			if (csgobj.leaf->geom) {
				auto prim = std::make_shared<OpenCSGPrim>(csgobj, OpenCSG::Subtraction);
				handles.push_back(prim);
				primitives.push_back(prim.get());
			}
		}
		if (primitives.size() > 1) {
			OpenCSG::render(primitives);
			glDepthFunc(GL_EQUAL);
		}

		for(const auto &csgobj : product.intersections) {
			const Color4f &c = csgobj.leaf->color;
			csgmode_e csgmode = CSGMODE_NORMAL;
			if (highlight_mode) csgmode = CSGMODE_HIGHLIGHT;
			if (background_mode) csgmode = CSGMODE_BACKGROUND;

			ColorMode colormode = COLORMODE_MATERIAL;
			if (highlight_mode) colormode = COLORMODE_HIGHLIGHT;
			if (background_mode) colormode = COLORMODE_BACKGROUND;

			setColor(colormode, c.data());
			glPushMatrix();
			glMultMatrixd(csgobj.leaf->matrix.data());
			render_surface(csgobj.leaf->geom, csgmode, csgobj.leaf->isMirrorMatrix());
			if (showedges)
				render_edges(csgobj.leaf->geom, csgmode);
			glPopMatrix();
		}
		for(const auto &csgobj : product.subtractions) {
			const Color4f &c = csgobj.leaf->color;
			csgmode_e csgmode = CSGMODE_DIFFERENCE;
			if (highlight_mode) csgmode = CSGMODE_HIGHLIGHT_DIFFERENCE;
			if (background_mode) csgmode = CSGMODE_BACKGROUND_DIFFERENCE;

			ColorMode colormode = COLORMODE_CUTOUT;
			if (highlight_mode) colormode = COLORMODE_HIGHLIGHT;
			if (background_mode) colormode = COLORMODE_BACKGROUND;

			setColor(colormode, c.data());
			glPushMatrix();
			glMultMatrixd(csgobj.leaf->matrix.data());
			render_surface(csgobj.leaf->geom, csgmode, csgobj.leaf->isMirrorMatrix());
			if (showedges)
				render_edges(csgobj.leaf->geom, csgmode);
			glPopMatrix();
		}

		glDepthFunc(GL_LEQUAL);
	}
#endif
}

BoundingBox OpenCSGRenderer::getBoundingBox() const
{
	BoundingBox bbox;
	if (this->root_products) bbox = this->root_products->getBoundingBox();
	if (this->highlights_products) bbox.extend(this->highlights_products->getBoundingBox());
	if (this->background_products) bbox.extend(this->background_products->getBoundingBox());
	return bbox;
}
