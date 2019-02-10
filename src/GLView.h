#pragma once

/* GLView: A basic OpenGL rectangle for rendering images.

This class is inherited by:

*QGLview - for Qt GUI
*OffscreenView - for offscreen rendering, in tests and from command-line
(This class is also overridden by NULLGL.cc for special experiments)

The view assumes either a Gimbal Camera (rotation,translation,distance)
or Vector Camera (eye,center/target) is being used. See Camera.h. The
cameras are not kept in sync.

QGLView only uses GimbalCamera while OffscreenView can use either one.
Some actions (showCrossHairs) only work properly on Gimbal Camera.

*/

#include "linalg.h"
#include "Camera.h"
#include "colormap.h"
#include <string>

struct Light
{
	bool enabled;
	bool fixedPos;
	bool fixedRot;
	Vector4d vec;
	Color4f color;

	Light() : enabled(false), fixedPos(false), fixedRot(false) { }
	Light(bool enabled, bool fixedPos, bool fixedRot, Vector4d vec, Color4f color) 
		: enabled(enabled), fixedPos(fixedPos), fixedRot(fixedRot), vec(vec), color(color) { }

	void setValue(const class ValuePtr &v);
	ValuePtr getValue() const;
};

class GLView
{
public:
	GLView();
	void setRenderer(class Renderer* r);
	Renderer *getRenderer() const { return this->renderer; }

	void initializeGL();
	void resizeGL(int w, int h);
	virtual void paintGL();

	// camera
	void setCamera(const class Camera &cam);

	// Lighting
	void setupDefaultLighting();
	class ValuePtr getLightValues() const;
	void setLightValues(const ValuePtr &v);

	// Color Scheme
	void setColorScheme(const ColorScheme &cs);
	void setColorScheme(const std::string &cs);
	void updateColorScheme();

	// Save image
	virtual bool save(const char *filename) = 0;

	// Info
	virtual std::string getRendererInfo() const = 0;
	virtual float getDPI() { return 1.0f; }

	Renderer *renderer;
	const ColorScheme *colorscheme;
	Camera cam;
	double far_far_away;
	size_t width;
	size_t height;
	double aspectratio;
	bool orthomode;
	bool showaxes;
	bool showfaces;
	bool showedges;
	bool showcrosshairs;
	bool showscale;

#ifdef ENABLE_OPENCSG
	bool is_opencsg_capable;
	virtual void display_opencsg_warning() = 0;
	bool opencsg_support;
	int opencsg_id;
#endif
protected:
	void showCrosshairs();
	void showAxes(const Color4f &col);
	void showSmallaxes(const Color4f &col);
	void showScalemarkers(const Color4f &col);
	void decodeMarkerValue(int di, double i, double l, int size_div_sm);
	void setupCamera();
	void setupGimbalCamera();
	void setupVectorCamera();
	void setupLighting();
	// lights - OpenGL guarantees at least 8
	static const int MAX_LIGHTS = 8;
	Light lights[MAX_LIGHTS];
};
