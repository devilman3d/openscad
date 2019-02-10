#include "polyset.h"
#include "polyset-utils.h"
#include "linalg.h"
#include "printutils.h"
#include "grid.h"
#include <Eigen/LU>
#include "Polygon2d-CGAL.h"

#ifndef NULLGL
static void gl_draw_triangle(const Vector3d &p0, const Vector3d &p1, const Vector3d &p2, double z, bool mirrored, bool normal = true)
{
	Vector3d v0 = p0 - p1;
	Vector3d v1 = p2 - p1;
	Vector3d norm = v1.cross(v0).normalized();
	if (normal)
		glNormal3d(norm[0], norm[1], norm[2]);

	glVertex3d(p0[0], p0[1], p0[2] + z);
	if (mirrored)
		glVertex3d(p1[0], p1[1], p1[2] + z);

	glVertex3d(p2[0], p2[1], p2[2] + z);
	if (!mirrored)
		glVertex3d(p1[0], p1[1], p1[2] + z);
}

void PolySet::render_surface(Renderer::csgmode_e csgmode, bool mirrored) const
{
	int listId = csgmode == Renderer::CSGMODE_NONE ? DisplayLists::None :
		(csgmode & CSGMODE_DIFFERENCE_FLAG) != 0 ?
		(mirrored ? DisplayLists::MirrorDiff : DisplayLists::NormalDiff) :
		(mirrored ? DisplayLists::Mirror : DisplayLists::Normal);
	if (displayLists[listId] != 0) {
		glCallList(displayLists[listId]);
		return;
	}
	displayLists[listId] = glGenLists(1);
	glNewList(displayLists[listId], GL_COMPILE_AND_EXECUTE);

	PRINTD("Polyset render");
	if (this->dim == 2) {
		// Render 2D objects 1mm thick, but differences slightly larger
		double zbase = 1 + ((csgmode & CSGMODE_DIFFERENCE_FLAG) ? 0.1 : 0);
		glBegin(GL_TRIANGLES);

		for (size_t i = 0; i < polygons.size(); i++) {
			const Polygon *poly = &polygons[i];
			if (poly->open)
				continue;
			// Render top+bottom
			for (double z = -zbase/2; z < zbase; z += zbase) {
				if (poly->size() == 3) {
					if (z < 0) {
						gl_draw_triangle(poly->at(0), poly->at(2), poly->at(1), z, mirrored);
					} else {
						gl_draw_triangle(poly->at(0), poly->at(1), poly->at(2), z, mirrored);
					}
				}
				else if (poly->size() == 4) {
					if (z < 0) {
						gl_draw_triangle(poly->at(0), poly->at(3), poly->at(1), z, mirrored);
						gl_draw_triangle(poly->at(2), poly->at(1), poly->at(3), z, mirrored);
					} else {
						gl_draw_triangle(poly->at(0), poly->at(1), poly->at(3), z, mirrored);
						gl_draw_triangle(poly->at(2), poly->at(3), poly->at(1), z, mirrored);
					}
				}
				else {
					Vector3d center = Vector3d::Zero();
					for (size_t j = 0; j < poly->size(); j++) {
						center[0] += poly->at(j)[0];
						center[1] += poly->at(j)[1];
					}
					center[0] /= poly->size();
					center[1] /= poly->size();
					for (size_t j = 1; j <= poly->size(); j++) {
						if (z < 0) {
							gl_draw_triangle(center, poly->at(j % poly->size()), poly->at(j - 1), z, mirrored);
						} else {
							gl_draw_triangle(center, poly->at(j - 1), poly->at(j % poly->size()), z, mirrored);
						}
					}
				}
			}
		}

		// Render sides
		if (polygon && polygon->outlines().size() > 0) {
			for (const Outline2d &o : polygon->outlines()) {
				for (size_t j = 1; j <= o.vertices.size(); j++) {
					if (o.open && j == o.vertices.size())
						continue;
					Vector3d p1(o.vertices[j-1][0], o.vertices[j-1][1], -zbase/2);
					Vector3d p2(o.vertices[j-1][0], o.vertices[j-1][1], zbase/2);
					Vector3d p3(o.vertices[j % o.vertices.size()][0], o.vertices[j % o.vertices.size()][1], -zbase/2);
					Vector3d p4(o.vertices[j % o.vertices.size()][0], o.vertices[j % o.vertices.size()][1], zbase/2);
					gl_draw_triangle(p2, p1, p3, 0, mirrored);
					gl_draw_triangle(p2, p3, p4, 0, mirrored);
				}
			}
		}
		else {
			// If we don't have borders, use the polygons as borders.
			// FIXME: When is this used?
			const Polygons *borders_p = &polygons;
			for (size_t i = 0; i < borders_p->size(); i++) {
				const Polygon *poly = &borders_p->at(i);
				for (size_t j = 1; j <= poly->size(); j++) {
					Vector3d p1 = poly->at(j - 1), p2 = poly->at(j - 1);
					Vector3d p3 = poly->at(j % poly->size()), p4 = poly->at(j % poly->size());
					p1[2] -= zbase/2, p2[2] += zbase/2;
					p3[2] -= zbase/2, p4[2] += zbase/2;
					gl_draw_triangle(p2, p1, p3, 0, mirrored);
					gl_draw_triangle(p2, p3, p4, 0, mirrored);
				}
			}
		}
		glEnd();
	} 
	else if (this->dim == 3) {
		for (size_t i = 0; i < polygons.size(); i++) {
			const Polygon *poly = &polygons[i];
			if (poly->open)
				continue;
			// don't generate normals when called from OpenCSG
			bool normals = csgmode != Renderer::CSGMODE_NONE;
			// use CW winding for front faces when called from OpenCSG
			if (csgmode == Renderer::CSGMODE_NONE)
				glFrontFace(GL_CW);
			glBegin(GL_TRIANGLES);
			if (poly->size() == 3) {
				gl_draw_triangle(poly->at(0), poly->at(1), poly->at(2), 0, mirrored, normals);
			}
			else if (poly->size() == 4) {
				gl_draw_triangle(poly->at(0), poly->at(1), poly->at(3), 0, mirrored, normals);
				gl_draw_triangle(poly->at(2), poly->at(3), poly->at(1), 0, mirrored, normals);
			}
			else {
				Vector3d center = Vector3d::Zero();
				for (size_t j = 0; j < poly->size(); j++) {
					center[0] += poly->at(j)[0];
					center[1] += poly->at(j)[1];
					center[2] += poly->at(j)[2];
				}
				center[0] /= poly->size();
				center[1] /= poly->size();
				center[2] /= poly->size();
				for (size_t j = 1; j <= poly->size(); j++) {
					gl_draw_triangle(center, poly->at(j - 1), poly->at(j % poly->size()), 0, mirrored, normals);
				}
			}
			glEnd();
			// reset to CCW winding for front faces
			if (csgmode == Renderer::CSGMODE_NONE)
				glFrontFace(GL_CCW);
		}
	}
	else {
		assert(false && "Cannot render object with no dimension");
	}

	glEndList();
}

/*! This is used in throwntogether and CGAL mode

	csgmode is set to CSGMODE_NONE in CGAL mode. In this mode a pure 2D rendering is performed.
*/
void PolySet::render_edges(Renderer::csgmode_e csgmode) const
{
	int listId = (csgmode & Renderer::CSGMODE_DIFFERENCE) != 0 ? DisplayLists::EdgesDiff : DisplayLists::Edges;
	if (displayLists[listId] != 0) {
		glCallList(displayLists[listId]);
		return;
	}
	displayLists[listId] = glGenLists(1);
	glNewList(displayLists[listId], GL_COMPILE_AND_EXECUTE);

	glDisable(GL_LIGHTING);
	if (this->dim == 2) {
		if (csgmode == Renderer::CSGMODE_NONE) {
			// Render skeleton
			if (auto skel = dynamic_pointer_cast<const Skelegon2d>(this->polygon)) {
				auto ec = [](Skelegon2d::Ss::Halfedge_handle e) {
					if (e->is_inner_bisector()) {
						glColor3f(1.0f, 0.0f, 0.0f);
					}
					else if (e->is_bisector()) {
						glColor3f(1.0f, 0.0f, 1.0f);
					}
					else if (e->is_border()) {
						glColor3f(1.0f, 1.0f, 1.0f);
					}
					else {
						glColor3f(0.5f, 0.5f, 0.5f);
					}
				};
				auto vc = [](Skelegon2d::Ss::Vertex_handle v) {
					if (v->is_split()) {
						glColor3f(1.0f, 1.0f, 0.0f);
					}
					else if (v->is_contour()) {
						glColor3f(0.0f, 1.0f, 0.0f);
					}
					else if (v->is_skeleton()) {
						glColor3f(0.0f, 0.0f, 1.0f);
					}
					else {
						glColor3f(1.0f, 1.0f, 1.0f);
					}
				};
				auto ss = skel->skeleton;
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LEQUAL);
				glLineWidth(2);
				for (auto fi = ss->faces_begin(); fi != ss->faces_end(); ++fi) {
					auto f = [&ec](Skelegon2d::Ss::Halfedge_handle e) {
						ec(e);
						auto v = e->vertex();
						glVertex3d(v->point().x(), v->point().y(), v->time());
					};
					glBegin(GL_POLYGON);
					auto ff = fi->halfedge();
					auto fp = ff;
					do {
						f(ff);
						ff = ff->next();
					} while (ff != fp);
					glEnd();
				}
				glLineWidth(5);
				glBegin(GL_LINES);
				for (auto fi = ss->faces_begin(); fi != ss->faces_end(); ++fi) {
					auto f = [&vc](Skelegon2d::Ss::Halfedge_handle e) {
						auto v = e->vertex();
						auto vv = e->next()->vertex();
						vc(v);
						glVertex3d(v->point().x(), v->point().y(), v->time());
						vc(vv);
						glVertex3d(vv->point().x(), vv->point().y(), vv->time());
					};
					auto e0 = fi->halfedge();
					auto e = e0;
					do {
						f(e);
						e = e->next();
					} while (e != e0);
				}
				glEnd();
			}
			else {
				// Render only outlines
				for (const Outline2d &o : polygon->outlines()) {
					glBegin(o.open ? GL_LINE_STRIP : GL_LINE_LOOP);
					for (const Vector2d &v : o.vertices) {
						glVertex3d(v[0], v[1], 0);
					}
					glEnd();
				}
			}
		}
		else {
			// Render 2D objects 1mm thick, but differences slightly larger
			double zbase = 1 + ((csgmode & CSGMODE_DIFFERENCE_FLAG) ? 0.1 : 0);

			for (const Outline2d &o : polygon->outlines()) {
				// Render top+bottom outlines
				for (double z = -zbase/2; z < zbase; z += zbase) {
					glBegin(o.open ? GL_LINE_STRIP : GL_LINE_LOOP);
					for (const Vector2d &v : o.vertices) {
						glVertex3d(v[0], v[1], z);
					}
					glEnd();
				}
				// Render sides
				//if (o.open)
				//	continue;
				glBegin(GL_LINES);
				for (const Vector2d &v : o.vertices) {
					glVertex3d(v[0], v[1], -zbase/2);
					glVertex3d(v[0], v[1], +zbase/2);
				}
				glEnd();
			}
		}
	} else if (dim == 3) {
		for (size_t i = 0; i < polygons.size(); i++) {
			const Polygon *poly = &polygons[i];
			glBegin(poly->open ? GL_LINE_STRIP : GL_LINE_LOOP);
			for (size_t j = 0; j < poly->size(); j++) {
				const Vector3d &p = poly->at(j);
				glVertex3d(p[0], p[1], p[2]);
			}
			glEnd();
		}
	}
	else {
		assert(false && "Cannot render object with no dimension");
	}
	glEnable(GL_LIGHTING);

	glEndList();
}


#else //NULLGL
static void gl_draw_triangle(const Vector3d &p0, const Vector3d &p1, const Vector3d &p2, double z, bool mirrored) {}
void PolySet::render_surface(Renderer::csgmode_e csgmode, bool mirrored) const {}
void PolySet::render_edges(Renderer::csgmode_e csgmode) const {}
#endif //NULLGL

