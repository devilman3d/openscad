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

#include "ThrownTogetherRenderer.h"
#include "polyset.h"
#include "printutils.h"

#include "system-gl.h"

ThrownTogetherRenderer::ThrownTogetherRenderer(shared_ptr<CSGProducts> root_products,
	shared_ptr<CSGProducts> highlight_products,
	shared_ptr<CSGProducts> background_products)
	: root_products(root_products)
	, highlight_products(highlight_products)
	, background_products(background_products)
{
}

void ThrownTogetherRenderer::draw(bool /*showfaces*/, bool showedges) const
{
	PRINTD("Thrown draw");
	if (this->root_products)
		renderCSGProducts(*this->root_products, false, false, showedges, true);
	if (this->background_products)
		renderCSGProducts(*this->background_products, false, true, showedges, false);
	if (this->highlight_products)
		renderCSGProducts(*this->highlight_products, true, false, showedges, false);
}

void ThrownTogetherRenderer::renderChainObject(const CSGChainObject &csgobj, bool highlight_mode, bool background_mode, bool showedges, bool fberror, OpenSCADOperator type) const
{
	if (this->geomVisitMark[std::make_pair(csgobj.leaf->geom.get(), &csgobj.leaf->matrix)]++ > 0) return;
	const Color4f &c = csgobj.leaf->color;
	csgmode_e csgmode = csgmode_e(
		(highlight_mode ?
			CSGMODE_HIGHLIGHT :
			(background_mode ? CSGMODE_BACKGROUND : CSGMODE_NORMAL)) |
			(type == OPENSCAD_DIFFERENCE ? CSGMODE_DIFFERENCE : CSGMODE_NONE));

	ColorMode colormode = COLORMODE_NONE;
	ColorMode edge_colormode = COLORMODE_NONE;

	if (highlight_mode) {
		colormode = COLORMODE_HIGHLIGHT;
		edge_colormode = COLORMODE_HIGHLIGHT_EDGES;
	}
	else if (background_mode) {
		if (csgobj.flags & CSGNode::FLAG_HIGHLIGHT) {
			colormode = COLORMODE_HIGHLIGHT;
		}
		else {
			colormode = COLORMODE_BACKGROUND;
		}
		edge_colormode = COLORMODE_BACKGROUND_EDGES;
	}
	else if (fberror) {
		if (csgobj.flags & CSGNode::FLAG_HIGHLIGHT) {
			colormode = COLORMODE_HIGHLIGHT;
		}
		else {
			colormode = COLORMODE_MATERIAL;
		}
		edge_colormode = COLORMODE_MATERIAL_EDGES;
	}
	else if (type == OPENSCAD_DIFFERENCE) {
		if (csgobj.flags & CSGNode::FLAG_HIGHLIGHT) {
			colormode = COLORMODE_HIGHLIGHT;
		}
		else {
			colormode = COLORMODE_CUTOUT;
		}
		edge_colormode = COLORMODE_CUTOUT_EDGES;
	}
	else {
		if (csgobj.flags & CSGNode::FLAG_HIGHLIGHT) {
			colormode = COLORMODE_HIGHLIGHT;
		}
		else {
			colormode = COLORMODE_MATERIAL;
		}
		edge_colormode = COLORMODE_MATERIAL_EDGES;
	}

	const Transform3d &m = csgobj.leaf->matrix;
	setColor(colormode, c.data());
	glPushMatrix();
	glMultMatrixd(m.data());
	glEnable(GL_CULL_FACE);
	if (fberror) {
		glDisable(GL_LIGHTING);
		glCullFace(GL_BACK);
		render_surface(csgobj.leaf->geom, csgmode, m.matrix().determinant() < 0);
		glCullFace(GL_FRONT);
		glColor4f(1.0f, 0.0f, 1.0f, c[3]);
		render_surface(csgobj.leaf->geom, csgmode, m.matrix().determinant() < 0);
		setColor(colormode, c.data());
	}
	else {
		//if (c[3] != 1) {
		//	glCullFace(GL_FRONT);
		//	render_surface(csgobj.leaf->geom, csgmode, m.matrix().determinant() < 0);
		//}
		glCullFace(GL_BACK);
		render_surface(csgobj.leaf->geom, csgmode, m.matrix().determinant() < 0);
	}
	if (showedges) {
		// FIXME? glColor4f((c[0]+1)/2, (c[1]+1)/2, (c[2]+1)/2, 1.0);
		setColor(edge_colormode);
		render_edges(csgobj.leaf->geom, csgmode);
	}
	glDisable(GL_CULL_FACE);
	glPopMatrix();

}

void ThrownTogetherRenderer::renderCSGProducts(const CSGProducts &products, bool highlight_mode, bool background_mode, bool showedges, bool fberror) const
{
	PRINTD("Thrown renderCSGProducts");
	glDepthFunc(GL_LEQUAL);
	this->geomVisitMark.clear();

	for (const auto &product : products.products) {
		for (const auto &csgobj : product.intersections) {
			renderChainObject(csgobj, highlight_mode, background_mode, showedges, fberror, OPENSCAD_INTERSECTION);
		}
		for (const auto &csgobj : product.subtractions) {
			renderChainObject(csgobj, highlight_mode, background_mode, showedges, fberror, OPENSCAD_DIFFERENCE);
		}
	}
}

BoundingBox ThrownTogetherRenderer::getBoundingBox() const
{
	BoundingBox bbox;
	if (this->root_products) bbox = this->root_products->getBoundingBox();
	if (this->highlight_products) bbox.extend(this->highlight_products->getBoundingBox());
	//	if (this->background_products) bbox.extend(this->background_products->getBoundingBox());
	return bbox;
}
