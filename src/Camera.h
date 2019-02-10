#pragma once

/*

Camera

For usage, see QGLView.cc, GLView.cc, export_png.cc, openscad.cc

There are two different types of cameras represented in this class:

*Gimbal camera - uses Euler Angles, object translation, and viewer distance
*Vector camera - uses 'eye', 'center', and 'up' vectors ('lookat' style)

Vector camera is a one-shot definition automatically converted to a 
gimbal camera where: trans = -center; rot and dist are computed

There are two modes of projection: Perspective and Orthogonal.

*/

#include "linalg.h"
#include <vector>

struct VectorCam
{
	// Vectorcam
	Vector3d eye;
	Vector3d center; // (aka 'target')
	Vector3d up;	 // computed
};

struct GimbalCam
{
	// Gimbalcam
	Vector3d object_trans;
	Vector3d object_rot;
	double viewer_distance;
};

class Camera
{
public:
	enum CameraType { NONE, GIMBAL, VECTOR } type;
	enum ProjectionType { ORTHOGONAL, PERSPECTIVE } projection;

	Camera(enum CameraType camtype = NONE);

	void setup(const GimbalCam &params);
	void setup(const VectorCam &params);

	void setProjection(ProjectionType type);

	void gimbalDefaultTranslate();

	void zoom(int delta);
	double zoomValue();

	void resetView();
	void viewAll(const BoundingBox &bbox);

	std::string statusText();

	// Vectorcam
	Vector3d eye;
	Vector3d center; // (aka 'target')
	Vector3d up; // not used currently

	// Gimbalcam
	Vector3d object_trans;
	Vector3d object_rot;
	double viewer_distance;

	// Perspective settings
	double fov; // Field of view

	// true if camera should try to view everything in a given
	// bounding box.
	bool viewall;

	// true if camera should point at center of bounding box
	// (normally it points at 0,0,0 or at given coordinates)
	bool autocenter;

	unsigned int pixel_width;
	unsigned int pixel_height;
};
