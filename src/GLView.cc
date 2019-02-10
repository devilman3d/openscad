#include "GLView.h"

#include "stdio.h"
#include "colormap.h"
#include "rendersettings.h"
#include "printutils.h"
#include "renderer.h"
#include "value.h"
#include "system-gl.h"
#include "value.h"
#include "modcontext.h"

#ifdef _WIN32
#include <GL/wglew.h>
#elif !defined(__APPLE__)
#include <GL/glxew.h>
#endif

#ifdef ENABLE_OPENCSG
#include <opencsg.h>
#endif

#include <cmath>
#include <boost/lexical_cast.hpp>

void Light::setValue(const ValuePtr &v)
{
	if (v->isDefinedAs(Value::STRUCT)) {
		ScopeContext sc(nullptr, v->toStruct());

		auto enabled = sc.lookup("enabled", true);
		if (enabled->isDefinedAs(Value::BOOL))
			this->enabled = enabled->toBool();

		auto fixedPos = sc.lookup("fixedPos", true);
		if (fixedPos->isDefinedAs(Value::BOOL))
			this->fixedPos = fixedPos->toBool();

		auto fixedRot = sc.lookup("fixedRot", true);
		if (fixedRot->isDefinedAs(Value::BOOL))
			this->fixedRot = fixedRot->toBool();

		auto vec = sc.lookup("vec", true);
		Vector4d vecd(this->vec[0], this->vec[1], this->vec[2], this->vec[3]);
		if (vec->getVec4(vecd[0], vecd[1], vecd[2], vecd[3]))
			this->vec = vecd;

		auto color = sc.lookup("color", true);
		Vector4d colord(this->color[0], this->color[1], this->color[2], this->color[3]);
		if (color->getVec4(colord[0], colord[1], colord[2], colord[3]))
			this->color = Color4f((float)colord[0], (float)colord[1], (float)colord[2], (float)colord[3]);
	}
}

ValuePtr Light::getValue() const
{
	Value::ScopeType scope;
	scope.addValue("enabled", enabled);
	scope.addValue("fixedPos", fixedPos);
	scope.addValue("fixedRot", fixedRot);
	scope.addValue("vec", vec);
	scope.addValue("color", color);
	return ValuePtr(scope);
}

GLView::GLView()
{
	showedges = false;
	showfaces = true;
	showaxes = false;
	showcrosshairs = false;
	showscale = false;
	renderer = NULL;
	colorscheme = &ColorMap::inst()->defaultColorScheme();
	cam = Camera();
	far_far_away = RenderSettings::inst()->far_gl_clip_limit;
#ifdef ENABLE_OPENCSG
	is_opencsg_capable = false;
	opencsg_support = true;
	static int sId = 0;
	this->opencsg_id = sId++;
#endif
}

void GLView::setRenderer(Renderer* r)
{
	renderer = r;
}

/* update the color schemes of the Renderer attached to this GLView
to match the colorscheme of this GLView.*/
void GLView::updateColorScheme()
{
	if (this->renderer) this->renderer->setColorScheme(*this->colorscheme);
}

/* change this GLView's colorscheme to the one given, and update the
Renderer attached to this GLView as well. */
void GLView::setColorScheme(const ColorScheme &cs)
{
	this->colorscheme = &cs;
	this->updateColorScheme();
}

void GLView::setColorScheme(const std::string &cs)
{
	const ColorScheme *colorscheme = ColorMap::inst()->findColorScheme(cs);
	if (colorscheme) {
		setColorScheme(*colorscheme);
	}
	else {
		PRINTB("WARNING: GLView: unknown colorscheme %s", cs);
	}
}

void GLView::resizeGL(int w, int h)
{
	cam.pixel_width = w;
	cam.pixel_height = h;
	glViewport(0, 0, w, h);
	aspectratio = w / h;
}

void GLView::setCamera(const Camera &cam)
{
	this->cam = cam;
}

void GLView::setupCamera()
{
	switch (this->cam.type) {
	case Camera::GIMBAL: {
		setupGimbalCamera();
		break;
	}
	case Camera::VECTOR: {
		setupVectorCamera();
		break;
	}
	default:
		break;
	}
}

void GLView::paintGL()
{
	glDisable(GL_LIGHTING);

	Color4f bgcol = ColorMap::getColor(*this->colorscheme, BACKGROUND_COLOR);
	Color4f axescolor = ColorMap::getColor(*this->colorscheme, AXES_COLOR);
	glClearColor(bgcol[0], bgcol[1], bgcol[2], 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	setupCamera();

	if (this->cam.type == Camera::GIMBAL) {
		// Only for GIMBAL cam
		if (showcrosshairs) GLView::showCrosshairs();
		// ...the axis lines need to follow the object translation.
		if (showaxes) GLView::showAxes(axescolor);
		// mark the scale along the axis lines
		if (showaxes && showscale) GLView::showScalemarkers(axescolor);
	}

	setupLighting();

	glEnable(GL_LIGHTING);
	glDepthFunc(GL_LESS);
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);
	glLineWidth(2);
	glColor3d(1.0, 0.0, 0.0);

	if (this->renderer) {
#if defined(ENABLE_OPENCSG)
		// FIXME: This belongs in the OpenCSG renderer, but it doesn't know about this ID yet
		OpenCSG::setContext(this->opencsg_id);
#endif
		this->renderer->draw(showfaces, showedges);
	}

	// Only for GIMBAL
	glDisable(GL_LIGHTING);
	if (showaxes) GLView::showSmallaxes(axescolor);
}

void GLView::initializeGL()
{
	glEnable(GL_DEPTH_TEST);
	glDepthRange(-far_far_away, +far_far_away);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	setupLighting();

	glEnable(GL_LIGHTING);
	glEnable(GL_NORMALIZE);

	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	// The following line is reported to fix issue #71
	glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 63);
	glEnable(GL_COLOR_MATERIAL);
}

void GLView::showSmallaxes(const Color4f &col)
{
	// Fixme - this doesnt work in Vector Camera mode

	float dpi = this->getDPI();
	// Small axis cross in the lower left corner
	glDepthFunc(GL_LEQUAL);

	// Set up an orthographic projection of the axis cross in the corner
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glTranslatef(-0.8f, -0.8f, 0.0f);
	double scale = 90;
	glOrtho(-scale * dpi*aspectratio, scale*dpi*aspectratio,
		-scale * dpi, scale*dpi,
		-scale * dpi, scale*dpi);
	gluLookAt(0.0, -1.0, 0.0,
		0.0, 0.0, 0.0,
		0.0, 0.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	auto rot = cam.object_rot;
	glRotated(rot.x(), 1.0, 0.0, 0.0);
	glRotated(rot.y(), 0.0, 1.0, 0.0);
	glRotated(rot.z(), 0.0, 0.0, 1.0);

	glLineWidth(dpi);
	glBegin(GL_LINES);
	glColor3d(1.0, 0.0, 0.0);
	glVertex3d(0, 0, 0); glVertex3d(10 * dpi, 0, 0); // x axis
	glVertex3d(0, 5 * dpi, 0); glVertex3d(5 * dpi, 5 * dpi, 0);  // xy plane
	glVertex3d(5 * dpi, 0, 0); glVertex3d(5 * dpi, 5 * dpi, 0);  // xy plane
	glColor3d(0.0, 1.0, 0.0);
	glVertex3d(0, 0, 0); glVertex3d(0, 10 * dpi, 0); // y axis
	glColor3d(0.0, 0.0, 1.0);
	glVertex3d(0, 0, 0); glVertex3d(0, 0, 10 * dpi); // z axis
	glEnd();

	GLdouble mat_model[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, mat_model);

	GLdouble mat_proj[16];
	glGetDoublev(GL_PROJECTION_MATRIX, mat_proj);

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	GLdouble xlabel_x, xlabel_y, xlabel_z;
	gluProject(12 * dpi, 0, 0, mat_model, mat_proj, viewport, &xlabel_x, &xlabel_y, &xlabel_z);
	xlabel_x = std::round(xlabel_x); xlabel_y = std::round(xlabel_y);

	GLdouble ylabel_x, ylabel_y, ylabel_z;
	gluProject(0, 12 * dpi, 0, mat_model, mat_proj, viewport, &ylabel_x, &ylabel_y, &ylabel_z);
	ylabel_x = std::round(ylabel_x); ylabel_y = std::round(ylabel_y);

	GLdouble zlabel_x, zlabel_y, zlabel_z;
	gluProject(0, 0, 12 * dpi, mat_model, mat_proj, viewport, &zlabel_x, &zlabel_y, &zlabel_z);
	zlabel_x = std::round(zlabel_x); zlabel_y = std::round(zlabel_y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glTranslated(-1, -1, 0);
	glScaled(2.0 / viewport[2], 2.0 / viewport[3], 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glColor3f(col[0], col[1], col[2]);

	float d = 3 * dpi;
	glBegin(GL_LINES);
	// X Label
	glVertex3d(xlabel_x - d, xlabel_y - d, 0); glVertex3d(xlabel_x + d, xlabel_y + d, 0);
	glVertex3d(xlabel_x - d, xlabel_y + d, 0); glVertex3d(xlabel_x + d, xlabel_y - d, 0);
	// Y Label
	glVertex3d(ylabel_x - d, ylabel_y - d, 0); glVertex3d(ylabel_x + d, ylabel_y + d, 0);
	glVertex3d(ylabel_x - d, ylabel_y + d, 0); glVertex3d(ylabel_x, ylabel_y, 0);
	// Z Label
	glVertex3d(zlabel_x - d, zlabel_y - d, 0); glVertex3d(zlabel_x + d, zlabel_y - d, 0);
	glVertex3d(zlabel_x - d, zlabel_y + d, 0); glVertex3d(zlabel_x + d, zlabel_y + d, 0);
	glVertex3d(zlabel_x - d, zlabel_y - d, 0); glVertex3d(zlabel_x + d, zlabel_y + d, 0);
	// FIXME - depends on gimbal camera 'viewer distance'.. how to fix this
	//         for VectorCamera?
	glEnd();
}

void decodeMarkerValue(int di, double i, double l, int size_div_sm)
{
	// convert the axis position to a string
	std::ostringstream oss;
	oss << i;
	std::string digit = oss.str();

	// setup how far above the axis (or tic TBD) to draw the number
	double dig_buf = (l / size_div_sm) / 4;
	// setup the size of the character box
	double dig_w = (l / size_div_sm) / 2;
	double dig_h = (l / size_div_sm) + dig_buf;
	// setup the distance between characters
	double kern = dig_buf;
	double dig_wk = (dig_w)+kern;

	// set up ordering for different axes
	int ax[6][3] = {
		{0,1,2},
		{1,0,2},
		{1,2,0},
		{0,1,2},
		{1,0,2},
		{1,2,0} };

	// set up character vertex seqeunces for different axes
	int or_2[6][6] = {
		{0,1,3,2,4,5},
		{1,0,2,3,5,4},
		{1,0,2,3,5,4},
		{1,0,2,3,5,4},
		{0,1,3,2,4,5},
		{0,1,3,2,4,5} };

	int or_3[6][7] = {
		{0,1,3,2,3,5,4},
		{1,0,2,3,2,4,5},
		{1,0,2,3,2,4,5},
		{1,0,2,3,2,4,5},
		{0,1,3,2,3,5,4},
		{0,1,3,2,3,5,4} };

	int or_4[6][5] = {
		{0,2,3,1,5},
		{1,3,2,0,4},
		{1,3,2,0,4},
		{1,3,2,0,4},
		{0,2,3,1,5},
		{0,2,3,1,5} };

	int or_5[6][6] = {
		{1,0,2,3,5,4},
		{0,1,3,2,4,5},
		{0,1,3,2,4,5},
		{0,1,3,2,4,5},
		{1,0,2,3,5,4},
		{1,0,2,3,5,4} };

	int or_6[6][6] = {
		{1,0,4,5,3,2},
		{0,1,5,4,2,3},
		{0,1,5,4,2,3},
		{0,1,5,4,2,3},
		{1,0,4,5,3,2},
		{1,0,4,5,3,2} };

	int or_7[6][3] = {
		{0,1,4},
		{1,0,5},
		{1,0,5},
		{1,0,5},
		{0,1,4},
		{0,1,4} };

	int or_9[6][5] = {
		{5,1,0,2,3},
		{4,0,1,3,2},
		{4,0,1,3,2},
		{4,0,1,3,2},
		{5,1,0,2,3},
		{5,1,0,2,3} };

	int or_e[6][7] = {
		{1,0,2,3,2,4,5},
		{0,1,3,2,3,5,4},
		{0,1,3,2,3,5,4},
		{0,1,3,2,3,5,4},
		{1,0,2,3,2,4,5},
		{1,0,2,3,2,4,5} };

	std::string stash_digit = digit;

	// walk through axes
	//for (int di = 0; di < 6; di++) 
	{

		// setup negative axes
		double polarity = 1;
		if (di > 2) {
			polarity = -1;
			digit = "-" + stash_digit;
		}

		// fix the axes that need to run the opposite direction
		if (di > 0 && di < 4) {
			std::reverse(digit.begin(), digit.end());
		}

		// walk through and render the characters of the string
		for (std::string::size_type char_num = 0; char_num < digit.size(); ++char_num) {
			// setup the vertices for the char rendering based on the axis and position
			double dig_vrt[6][3] = {
				{polarity*((i + ((char_num)*dig_wk)) - (dig_w / 2)),dig_h,0},
				{polarity*((i + ((char_num)*dig_wk)) + (dig_w / 2)),dig_h,0},
				{polarity*((i + ((char_num)*dig_wk)) - (dig_w / 2)),dig_h / 2 + dig_buf,0},
				{polarity*((i + ((char_num)*dig_wk)) + (dig_w / 2)),dig_h / 2 + dig_buf,0},
				{polarity*((i + ((char_num)*dig_wk)) - (dig_w / 2)),dig_buf,0},
				{polarity*((i + ((char_num)*dig_wk)) + (dig_w / 2)),dig_buf,0} };

			// convert the char into lines appropriate for the axis being used
			// psuedo 7 segment vertices are:
			// A--B
			// |  |
			// C--D
			// |  |
			// E--F
			switch (digit[char_num]) {
			case '1':
				glBegin(GL_LINES);
				glVertex3d(dig_vrt[0][ax[di][0]], dig_vrt[0][ax[di][1]], dig_vrt[0][ax[di][2]]);  //a
				glVertex3d(dig_vrt[4][ax[di][0]], dig_vrt[4][ax[di][1]], dig_vrt[4][ax[di][2]]);  //e
				glEnd();
				break;

			case '2':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[or_2[di][0]][ax[di][0]], dig_vrt[or_2[di][0]][ax[di][1]], dig_vrt[or_2[di][0]][ax[di][2]]);  //a
				glVertex3d(dig_vrt[or_2[di][1]][ax[di][0]], dig_vrt[or_2[di][1]][ax[di][1]], dig_vrt[or_2[di][1]][ax[di][2]]);  //b
				glVertex3d(dig_vrt[or_2[di][2]][ax[di][0]], dig_vrt[or_2[di][2]][ax[di][1]], dig_vrt[or_2[di][2]][ax[di][2]]);  //d
				glVertex3d(dig_vrt[or_2[di][3]][ax[di][0]], dig_vrt[or_2[di][3]][ax[di][1]], dig_vrt[or_2[di][3]][ax[di][2]]);  //c
				glVertex3d(dig_vrt[or_2[di][4]][ax[di][0]], dig_vrt[or_2[di][4]][ax[di][1]], dig_vrt[or_2[di][4]][ax[di][2]]);  //e
				glVertex3d(dig_vrt[or_2[di][5]][ax[di][0]], dig_vrt[or_2[di][5]][ax[di][1]], dig_vrt[or_2[di][5]][ax[di][2]]);  //f
				glEnd();
				break;

			case '3':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[or_3[di][0]][ax[di][0]], dig_vrt[or_3[di][0]][ax[di][1]], dig_vrt[or_3[di][0]][ax[di][2]]);  //a
				glVertex3d(dig_vrt[or_3[di][1]][ax[di][0]], dig_vrt[or_3[di][1]][ax[di][1]], dig_vrt[or_3[di][1]][ax[di][2]]);  //b
				glVertex3d(dig_vrt[or_3[di][2]][ax[di][0]], dig_vrt[or_3[di][2]][ax[di][1]], dig_vrt[or_3[di][2]][ax[di][2]]);  //d
				glVertex3d(dig_vrt[or_3[di][3]][ax[di][0]], dig_vrt[or_3[di][3]][ax[di][1]], dig_vrt[or_3[di][3]][ax[di][2]]);  //c
				glVertex3d(dig_vrt[or_3[di][4]][ax[di][0]], dig_vrt[or_3[di][4]][ax[di][1]], dig_vrt[or_3[di][4]][ax[di][2]]);  //d
				glVertex3d(dig_vrt[or_3[di][5]][ax[di][0]], dig_vrt[or_3[di][5]][ax[di][1]], dig_vrt[or_3[di][5]][ax[di][2]]);  //f
				glVertex3d(dig_vrt[or_3[di][6]][ax[di][0]], dig_vrt[or_3[di][6]][ax[di][1]], dig_vrt[or_3[di][6]][ax[di][2]]);  //e
				glEnd();
				break;

			case '4':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[or_4[di][0]][ax[di][0]], dig_vrt[or_4[di][0]][ax[di][1]], dig_vrt[or_4[di][0]][ax[di][2]]);  //a
				glVertex3d(dig_vrt[or_4[di][1]][ax[di][0]], dig_vrt[or_4[di][1]][ax[di][1]], dig_vrt[or_4[di][1]][ax[di][2]]);  //c
				glVertex3d(dig_vrt[or_4[di][2]][ax[di][0]], dig_vrt[or_4[di][2]][ax[di][1]], dig_vrt[or_4[di][2]][ax[di][2]]);  //d
				glVertex3d(dig_vrt[or_4[di][3]][ax[di][0]], dig_vrt[or_4[di][3]][ax[di][1]], dig_vrt[or_4[di][3]][ax[di][2]]);  //b
				glVertex3d(dig_vrt[or_4[di][4]][ax[di][0]], dig_vrt[or_4[di][4]][ax[di][1]], dig_vrt[or_4[di][4]][ax[di][2]]);  //f
				glEnd();
				break;

			case '5':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[or_5[di][0]][ax[di][0]], dig_vrt[or_5[di][0]][ax[di][1]], dig_vrt[or_5[di][0]][ax[di][2]]);  //b
				glVertex3d(dig_vrt[or_5[di][1]][ax[di][0]], dig_vrt[or_5[di][1]][ax[di][1]], dig_vrt[or_5[di][1]][ax[di][2]]);  //a
				glVertex3d(dig_vrt[or_5[di][2]][ax[di][0]], dig_vrt[or_5[di][2]][ax[di][1]], dig_vrt[or_5[di][2]][ax[di][2]]);  //c
				glVertex3d(dig_vrt[or_5[di][3]][ax[di][0]], dig_vrt[or_5[di][3]][ax[di][1]], dig_vrt[or_5[di][3]][ax[di][2]]);  //d
				glVertex3d(dig_vrt[or_5[di][4]][ax[di][0]], dig_vrt[or_5[di][4]][ax[di][1]], dig_vrt[or_5[di][4]][ax[di][2]]);  //f
				glVertex3d(dig_vrt[or_5[di][5]][ax[di][0]], dig_vrt[or_5[di][5]][ax[di][1]], dig_vrt[or_5[di][5]][ax[di][2]]);  //e
				glEnd();
				break;

			case '6':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[or_6[di][0]][ax[di][0]], dig_vrt[or_6[di][0]][ax[di][1]], dig_vrt[or_6[di][0]][ax[di][2]]);  //b
				glVertex3d(dig_vrt[or_6[di][1]][ax[di][0]], dig_vrt[or_6[di][1]][ax[di][1]], dig_vrt[or_6[di][1]][ax[di][2]]);  //a
				glVertex3d(dig_vrt[or_6[di][2]][ax[di][0]], dig_vrt[or_6[di][2]][ax[di][1]], dig_vrt[or_6[di][2]][ax[di][2]]);  //e
				glVertex3d(dig_vrt[or_6[di][3]][ax[di][0]], dig_vrt[or_6[di][3]][ax[di][1]], dig_vrt[or_6[di][3]][ax[di][2]]);  //f
				glVertex3d(dig_vrt[or_6[di][4]][ax[di][0]], dig_vrt[or_6[di][4]][ax[di][1]], dig_vrt[or_6[di][4]][ax[di][2]]);  //d
				glVertex3d(dig_vrt[or_6[di][5]][ax[di][0]], dig_vrt[or_6[di][5]][ax[di][1]], dig_vrt[or_6[di][5]][ax[di][2]]);  //c
				glEnd();
				break;

			case '7':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[or_7[di][0]][ax[di][0]], dig_vrt[or_7[di][0]][ax[di][1]], dig_vrt[or_7[di][0]][ax[di][2]]);  //a
				glVertex3d(dig_vrt[or_7[di][1]][ax[di][0]], dig_vrt[or_7[di][1]][ax[di][1]], dig_vrt[or_7[di][1]][ax[di][2]]);  //b
				glVertex3d(dig_vrt[or_7[di][2]][ax[di][0]], dig_vrt[or_7[di][2]][ax[di][1]], dig_vrt[or_7[di][2]][ax[di][2]]);  //e
				glEnd();
				break;

			case '8':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[2][ax[di][0]], dig_vrt[2][ax[di][1]], dig_vrt[2][ax[di][2]]);  //c
				glVertex3d(dig_vrt[3][ax[di][0]], dig_vrt[3][ax[di][1]], dig_vrt[3][ax[di][2]]);  //d
				glVertex3d(dig_vrt[1][ax[di][0]], dig_vrt[1][ax[di][1]], dig_vrt[1][ax[di][2]]);  //b
				glVertex3d(dig_vrt[0][ax[di][0]], dig_vrt[0][ax[di][1]], dig_vrt[0][ax[di][2]]);  //a
				glVertex3d(dig_vrt[4][ax[di][0]], dig_vrt[4][ax[di][1]], dig_vrt[4][ax[di][2]]);  //e
				glVertex3d(dig_vrt[5][ax[di][0]], dig_vrt[5][ax[di][1]], dig_vrt[5][ax[di][2]]);  //f
				glVertex3d(dig_vrt[3][ax[di][0]], dig_vrt[3][ax[di][1]], dig_vrt[3][ax[di][2]]);  //d
				glEnd();
				break;

			case '9':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[or_9[di][0]][ax[di][0]], dig_vrt[or_9[di][0]][ax[di][1]], dig_vrt[or_9[di][0]][ax[di][2]]);  //f
				glVertex3d(dig_vrt[or_9[di][1]][ax[di][0]], dig_vrt[or_9[di][1]][ax[di][1]], dig_vrt[or_9[di][1]][ax[di][2]]);  //b
				glVertex3d(dig_vrt[or_9[di][2]][ax[di][0]], dig_vrt[or_9[di][2]][ax[di][1]], dig_vrt[or_9[di][2]][ax[di][2]]);  //a
				glVertex3d(dig_vrt[or_9[di][3]][ax[di][0]], dig_vrt[or_9[di][3]][ax[di][1]], dig_vrt[or_9[di][3]][ax[di][2]]);  //c
				glVertex3d(dig_vrt[or_9[di][4]][ax[di][0]], dig_vrt[or_9[di][4]][ax[di][1]], dig_vrt[or_9[di][4]][ax[di][2]]);  //d
				glEnd();
				break;

			case '0':
				glBegin(GL_LINE_LOOP);
				glVertex3d(dig_vrt[0][ax[di][0]], dig_vrt[0][ax[di][1]], dig_vrt[0][ax[di][2]]);  //a
				glVertex3d(dig_vrt[1][ax[di][0]], dig_vrt[1][ax[di][1]], dig_vrt[1][ax[di][2]]);  //b
				glVertex3d(dig_vrt[5][ax[di][0]], dig_vrt[5][ax[di][1]], dig_vrt[5][ax[di][2]]);  //f
				glVertex3d(dig_vrt[4][ax[di][0]], dig_vrt[4][ax[di][1]], dig_vrt[4][ax[di][2]]);  //e
				glEnd();
				break;

			case '-':
				glBegin(GL_LINES);
				glVertex3d(dig_vrt[2][ax[di][0]], dig_vrt[2][ax[di][1]], dig_vrt[2][ax[di][2]]);  //c
				glVertex3d(dig_vrt[3][ax[di][0]], dig_vrt[3][ax[di][1]], dig_vrt[3][ax[di][2]]);  //d
				glEnd();
				break;

			case '.':
				glBegin(GL_LINES);
				glVertex3d(dig_vrt[4][ax[di][0]], dig_vrt[4][ax[di][1]], dig_vrt[4][ax[di][2]]);  //e
				glVertex3d(dig_vrt[5][ax[di][0]], dig_vrt[5][ax[di][1]], dig_vrt[5][ax[di][2]]);  //f
				glEnd();
				break;

			case 'e':
				glBegin(GL_LINE_STRIP);
				glVertex3d(dig_vrt[or_e[di][0]][ax[di][0]], dig_vrt[or_e[di][0]][ax[di][1]], dig_vrt[or_e[di][0]][ax[di][2]]);  //b
				glVertex3d(dig_vrt[or_e[di][1]][ax[di][0]], dig_vrt[or_e[di][1]][ax[di][1]], dig_vrt[or_e[di][1]][ax[di][2]]);  //a
				glVertex3d(dig_vrt[or_e[di][2]][ax[di][0]], dig_vrt[or_e[di][2]][ax[di][1]], dig_vrt[or_e[di][2]][ax[di][2]]);  //c
				glVertex3d(dig_vrt[or_e[di][3]][ax[di][0]], dig_vrt[or_e[di][3]][ax[di][1]], dig_vrt[or_e[di][3]][ax[di][2]]);  //d
				glVertex3d(dig_vrt[or_e[di][4]][ax[di][0]], dig_vrt[or_e[di][4]][ax[di][1]], dig_vrt[or_e[di][4]][ax[di][2]]);  //c
				glVertex3d(dig_vrt[or_e[di][5]][ax[di][0]], dig_vrt[or_e[di][5]][ax[di][1]], dig_vrt[or_e[di][5]][ax[di][2]]);  //e
				glVertex3d(dig_vrt[or_e[di][6]][ax[di][0]], dig_vrt[or_e[di][6]][ax[di][1]], dig_vrt[or_e[di][6]][ax[di][2]]);  //f
				glEnd();
				break;

			}
		}
	}
}

void drawTick(const Vector3d &axis, const Vector3d &perp, double value, double len)
{
	if (value < 0) {
		glPushAttrib(GL_LINE_BIT);
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(3, 0xAAAA);
	}

	Vector3d v0 = axis * value;
	Vector3d v1 = axis * value + perp * len;

	glBegin(GL_LINES);

	glVertex3d(v0[0], v0[1], v0[2]); glVertex3d(v1[0], v1[1], v1[2]); // 1 arm
	//glVertex3d(i,-l/size_div,0); glVertex3d(i,l/size_div,0); // 2 arms
	//glVertex3d(i,0,-l/size_div); glVertex3d(i,0,l/size_div); // 4 arms (w/ 2 arms line)

	glEnd();

	if (value < 0) {
		glPopAttrib();
	}
}

struct Axes
{
	Axes(double l, const Vector3d &origin) : l(l)
	{
		log_l = (int)log10(l);
		j = pow(10, log_l - (l < 1.5 ? 2 : 1));
		num_ticks = l / j;
		major_tick = std::max(1, (int)(j * (num_ticks < 50 ? 5 : 10)));
		this->origin[0] = (int)(origin.x() / j) * j;
		this->origin[1] = (int)(origin.y() / j) * j;
		this->origin[2] = (int)(origin.z() / j) * j;
	}

	// the length of the axis
	double l;
	// the origin of the view
	Vector3d origin;
	// the log10 of l
	int log_l;
	// j represents the increment for each minor tic
	double j;
	// the number of visible ticks
	int num_ticks;
	// the spacing of major ticks
	int major_tick;

	const int size_div_sm = 60;       // divisor for l to determine minor tic size
	const int size_div_lg = 30;       // divisor for l to determine minor tic size

	void drawTicks()
	{
		for (int i = -num_ticks; i < num_ticks; ++i) {
			double ii = i * j;
			for (int axis = 0; axis < 3; ++axis) {
				double ox = ii - origin[axis];
				if (ox != 0) { // non-zero tick
					// set the minor tic to the standard size
					int size_div_x = size_div_sm;
					if (((int)ox % major_tick) == 0) {      // major tic
						size_div_x = size_div_sm * .5; // resize to a major tic
						decodeMarkerValue(ox < 0 ? axis + 3 : axis, std::abs(ox), l, size_div_sm);    // print number
					}
					Vector3d tickAxis = axis == 0 ? Vector3d::UnitX() : axis == 1 ? Vector3d::UnitY() : Vector3d::UnitZ();
					Vector3d tickPerp = axis == 0 ? Vector3d::UnitY() : axis == 1 ? Vector3d::UnitX() : Vector3d::UnitX();
					drawTick(tickAxis, -tickPerp, ox, l / size_div_x); // draw tick
				}
			}
		}
	}

	void drawLine(const Vector3d &axis, double t0, double t1)
	{
		if (t1 < 0)
			t1 = 0;
		if (t0 > 0)
			t0 = 0;

		Vector3d v0 = axis * t0;
		Vector3d vm = axis * 0;
		Vector3d v1 = axis * t1;

		if (t0 < 0) {
			glPushAttrib(GL_LINE_BIT);
			glEnable(GL_LINE_STIPPLE);
			glLineStipple(3, 0xAAAA);

			glBegin(GL_LINES);
			glVertex3d(v0[0], v0[1], v0[2]);
			glVertex3d(vm[0], vm[1], vm[2]);
			glEnd();

			glPopAttrib();
		}
		if (t1 > 0) {
			glBegin(GL_LINES);
			glVertex3d(vm[0], vm[1], vm[2]);
			glVertex3d(v1[0], v1[1], v1[2]);
			glEnd();
		}
	}

	void drawLines()
	{
		Vector3d n = -origin - Vector3d(l, l, l);
		Vector3d x = -origin + Vector3d(l, l, l);

		drawLine(Vector3d::UnitX(), n[0], x[0]);
		drawLine(Vector3d::UnitY(), n[1], x[1]);
		drawLine(Vector3d::UnitZ(), n[2], x[2]);
	}
};

void GLView::showCrosshairs()
{
	// FIXME: this might not work with Vector camera
	glPushMatrix();
	// The crosshair should be fixed at the center of the viewport...
	glTranslated(-cam.object_trans.x(), -cam.object_trans.y(), -cam.object_trans.z());

	glLineWidth(this->getDPI());
	Color4f col = ColorMap::getColor(*this->colorscheme, CROSSHAIR_COLOR);
	glColor3f(col[0], col[1], col[2]);
	glBegin(GL_LINES);
	for (double xf = -1; xf <= +1; xf += 2)
		for (double yf = -1; yf <= +1; yf += 2) {
			double vd = cam.zoomValue() / 8;
			glVertex3d(-xf * vd, -yf * vd, -vd);
			glVertex3d(+xf * vd, +yf * vd, +vd);
		}
	glEnd();

	glPopMatrix();
}

void GLView::showAxes(const Color4f &col)
{
	// FIXME: doesn't work under Vector Camera
	// Large gray axis cross inline with the model
	double l = cam.zoomValue();
	glLineWidth(this->getDPI());
	glColor3f(col[0], col[1], col[2]);

	Axes axes(l, cam.object_trans);
	axes.drawLines();
}

void GLView::showScalemarkers(const Color4f &col)
{
	// Add scale tics on large axes
	double l = cam.zoomValue();
	glLineWidth(this->getDPI());
	glColor3f(col[0], col[1], col[2]);

	Axes axes(l, cam.object_trans);
	axes.drawTicks();
}

void GLView::setupGimbalCamera()
{
	double dist = cam.zoomValue();

	// begin projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	switch (this->cam.projection) {
	case Camera::PERSPECTIVE: {
		gluPerspective(cam.fov, aspectratio, 0.1*dist, 100 * dist);
		break;
	}
	case Camera::ORTHOGONAL: {
		double height = dist * tan(cam.fov / 2 * M_PI / 180);
		glOrtho(-height * aspectratio, height*aspectratio,
			-height, height,
			-100 * dist, +100 * dist);
		break;
	}
	}
	// postfix to projection matrix?
	gluLookAt(0.0, -dist, 0.0,
		0.0, 0.0, 0.0,
		0.0, 0.0, 1.0);

	// begin modelview
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// rotate around origin
	glRotated(cam.object_rot.x(), 1.0, 0.0, 0.0);
	glRotated(cam.object_rot.y(), 0.0, 1.0, 0.0);
	glRotated(cam.object_rot.z(), 0.0, 0.0, 1.0);

	// translate the scene
	glTranslated(cam.object_trans.x(), cam.object_trans.y(), cam.object_trans.z());
}

void GLView::setupVectorCamera()
{
	Vector3d dir(cam.center - cam.eye);
	double dist = dir.norm();

	// begin projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	switch (this->cam.projection) {
	case Camera::PERSPECTIVE: {
		gluPerspective(cam.fov, aspectratio, 0.1*dist, 100 * dist);
		break;
	}
	case Camera::ORTHOGONAL: {
		double height = dist * tan(cam.fov / 2 * M_PI / 180);
		glOrtho(-height * aspectratio, height*aspectratio,
			-height, height,
			-100 * dist, +100 * dist);
		break;
	}
	}

	// begin modelview
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	Vector3d up(0.0, 0.0, 1.0);
	if (dir.cross(up).norm() < 0.001) { // View direction is ~parallel with up vector
		up << 0.0, 1.0, 0.0;
	}

	// affixed to modelview
	gluLookAt(cam.eye[0], cam.eye[1], cam.eye[2],
		cam.center[0], cam.center[1], cam.center[2],
		up[0], up[1], up[2]);
}


namespace LightDefaults
{
	Light value[] =
	{
		Light(true, false, false, Vector4d(0.0, -2.0, -1.0, 0), Color4f(0.5f, 0.5f, 0.5f, 1.0f)),
		Light(true, false, false, Vector4d(2.0, -1.0,  1.0, 0), Color4f(0.5f, 0.5f, 0.5f, 1.0f)),
		Light(true, false, false, Vector4d(-1.0, -2.0,  1.0, 0), Color4f(0.5f, 0.5f, 0.5f, 1.0f))
	};

	const size_t NUM_VALUES = size(value);
}

void GLView::setupDefaultLighting()
{
	// setup default lighting
	for (int i = 0; i < LightDefaults::NUM_VALUES; ++i) {
		lights[i] = LightDefaults::value[i];
	}
	for (int i = LightDefaults::NUM_VALUES; i < MAX_LIGHTS; ++i) {
		lights[i] = Light();
	}
}

void GLView::setupLighting()
{
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		if (lights[i].enabled) {
			glPushMatrix();
			if (lights[i].fixedPos) {
				// translate the light to the origin
				glTranslated(-cam.object_trans.x(), -cam.object_trans.y(), -cam.object_trans.z());
			}
			if (!lights[i].fixedRot) {
				// rotate the light in eye space
				glRotated(-cam.object_rot.z(), 0, 0, 1.0);
				glRotated(-cam.object_rot.y(), 0, 1.0, 0);
				glRotated(-cam.object_rot.x(), 1.0, 0, 0);
			}

			Vector4d pos = lights[i].vec;
			GLfloat light_position[] = { (GLfloat)pos[0], (GLfloat)pos[1], (GLfloat)pos[2], (GLfloat)abs(pos[3]) };
			glLightfv(GL_LIGHT0 + i, GL_POSITION, light_position);

			Color4f col = lights[i].color;
			GLfloat light_color[] = { (GLfloat)col[0], (GLfloat)col[1], (GLfloat)col[2], (GLfloat)col[3] };
			glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, light_color);

			glEnable(GL_LIGHT0 + i);

			glPopMatrix();
		}
		else
			glDisable(GL_LIGHT0 + i);
	}
}

class ValuePtr GLView::getLightValues() const
{
	Value::VectorType result;
	for (size_t i = 0; i < size(lights); ++i)
		result.push_back(lights[i].getValue());
	return ValuePtr(result);
}

void GLView::setLightValues(const ValuePtr &v)
{
	if (v->isDefinedAs(Value::VECTOR)) {
		auto &vv = v->toVector();
		for (int i = 0; i < vv.size(); ++i) {
			lights[i].setValue(vv[i]);
		}
	}
}
