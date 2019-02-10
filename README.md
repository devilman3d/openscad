[![Travis CI](https://api.travis-ci.org/openscad/openscad.png)](https://travis-ci.org/openscad/openscad)
[![Coverity Status](https://scan.coverity.com/projects/2510/badge.svg)](https://scan.coverity.com/projects/2510)

# What is OpenSCAD?
[![Flattr this git repo](http://api.flattr.com/button/flattr-badge-large.png)](https://flattr.com/submit/auto?user_id=openscad&url=http://openscad.org&title=OpenSCAD&language=&tags=github&category=software)

OpenSCAD is a software for creating solid 3D CAD objects. It is free software
and available for Linux/UNIX, MS Windows and Mac OS X.

This is my heavily customized version of OpenSCAD. See below for details.

## Extras in my version that make it fast:
 * Hardened multi-threaded CGAL rendering
 * Atomic operations on Gmp numeric types
 * Per-CPU progress/utilization indicators
 * Optimized group operations
 * Optimized geometry caching
 * Optimized automatic polyset <=> nef conversion
 * No final implicit union
 * Simplified OpenGL rendering

## Enhancements to the SCAD language:
### User functions with local scope:
```
function ExampleFunction([arg1 [= default_value1]...])
{
	// expressions that are evaluated in order
	a = sin(arg1);
	// final "return" is required!
	return a;
}
```
### User struct definitions:
```
struct ExampleStruct
{
	// expressions are evaluated in order
	l = 50; w = 25; area = l * w;

	// nested struct definition
	struct InnerStruct
	{
		h = 10;
		volume = area * h;
	}

	// values in structs are accessed with a dot (.)
	innerVolume = InnerStruct.volume;

	// structs can contain local function definitions
	function AFunction() = InnerStruct.volume;

	// structs can contain module definitions
	module AModule() { cube([l, w, InnerStruct.h]); }
}

// instances of structs can be created
ex = ExampleStruct;
ex.AModule();

// anonymous structs can be specified as inline expressions
ex2 = { r = 3; center = [1, 1]; };
translate(ex2.center)
	circle(ex2.r);

// structs can be passed as arguments to functions and modules
module ExampleModule(inputStruct)
{
	// ... this only compiles if it makes sense ...
	inputStruct.AModule();
}
ExampleModule(ex);

// structs can be aggregated with a plus (+)
ex3 = ex2 + { name = "I have a name!"; }
translate(ex3.center + [0, ex3.r]) text(ex3.name);

// structs are pseudo-polymorphic when aggregated
struct OverrideStruct
{
	r = ex3.r;

	// example to override ExampleStruct module definition
	module AModule()
	{
		intersection()
		{
			cube([l, w, InnerStruct.h]);
			sphere(r);
		}
	}
}
// aggregate and override...
ox = ExampleStruct + OverrideStruct;
// ... polymorphic inheritance magic!
ExampleModule(ox);
```
### New builtin variables:
 - $lights: file scope, read/write: an array of up to 8 anonymous structs with the following values:
   - enabled: enable/disable the light
   - fixedPos: don't move with the camera
   - fixedRot: don't rotate with the camera
   - vec: the OpenGL 4D position of the light
   - color: the light's color
```
$lights = [
  { enabled = true; vec = [0, 1, 0, 0]; color = [ 0.5, 0.5, 0.0, 1]; }, // yellow light pointing "forward" from the camera
  { enabled = true; vec = [0, 1, 1, 0]; color = [ 0.0, 0.5, 0.5, 1]; }, // cyan light pointing "forward+down" from the camera
  { enabled = false; } // disabled light
];
```
 - $world: read-only: a 4x4 matrix representing the current "world" transform (per parent modules)
 - $invWorld: read-only: a 4x4 matrix representing the inverted "world" transform
 - $debug: read/write: set true to emit debug information as geometries are evaluated
 - $convexity: read/write: as a config variable!

### New builtin functions:
 - identity(): Returns an identity matrix.
 - inverse(m): Returns the inverse of _m_. _m_ must be a 4x4 transform matrix.
 - rotation(): Returns a rotation matrix. Accepts the same arguments as the rotate() module.
 - translation(): Returns a translation matrix. Accepts the same arguments as the translate() module.
 - scaling(): Returns a scaling matrix. Accepts the same arguments as the scale() module.
 - lerp(a, b, t): Returns the linear interopolation: (1 - t) * a + t * b
 - lookup(s): Enhanced to lookup a local variable or config variable when _s_ is a string. 
   Only simple variable names are supported, e.g. no dots (.)

### New builtin modules:
 - help(): prints a list of all builtin modules and functions
 - nef(): explicitly convert child geometries to CGAL Nef Polyhedrons
 - polyset(): explicitly convert child geometries to PolySets
 - glide(points[, paths]): perform CGAL "glide" operation using child geometries
   - _points_ is a required array of 3D points defining the path for the children
   - _paths_ is optional. If specified, it has two options: 
     - an array of indices into _points_ defining the path to glide (single path)
     - an array of arrays of indicies into _points_ (mutliple paths)
   - NOTE: if the first and last point of any path are coincident, a CGAL error will occur
 - center(): translates the children to the center of their combined bounding box
 - boundingbox(): creates a "cube" geometry completely containing the children
 - boundingsphere(): creates a "sphere" geometry completely containing the children
 - line(points): creates a non-geometry line; only visible with show edges
   - can be transformed like any child geometry with parent modules
   - can be colored in Preview using a color() parent module
   - _points_ is an array of 2 or more 3D points
 
## Codebase and functional changes:
This version should compile and run the same as a regular version.

I have attempted to maintain complete compatibility with existing SCAD code.
Functions and modules marked deprecated in the standard have been removed.
To my knowledge all existing code should continue to work as-is in this version.
Due to the optimizations with nef/polyset and grouping, there will be visual differences (also, the new lighting model).
Explicit nef(), polyset() and union() can be used to force the desired output.
Particularly, before exporting a model for 3D printing, consider an explicit union() to ensure it is manifold.
Note: .stl export has been enhanced to export multiple objects into one file.
It's just a triangle soup...slicers don't seem to care as long as the meshes do not intersect.

The Module, Node and Context class heirarchies have been completely refactored. It is 
much easier to add new node types (SCAD modules) to the C++ codebase. I have converted 
most of the codebase to use FactoryNode and FactoryModule<> classes. Due to this ease 
of integration, there are many more new builtin modules than listed above. They don't 
work for anything useful (skeleton, roof) or don't work at all, (cdifference, 
cintersection)...just experiments, nothing to see here...yet.

The parameters UI is disabled by default. I don't use it and any change to any core
class was wasting 1-2 minutes of my life to recompile. If you want it, fix it...

## How to Build and Debug on Windows with WSL
- You should be comfortable building and debugging other simple Linux programs with VisualStudio and WSL!
- You should be able to build openscad from your WSL Bash terminal following the normal instructions for a Linux build. It is surprisingly easy compared to MingW...only a couple extra dependencies to install (sorry, but I don't remember which)
- You should have an X Server running on Windows and WSL is setup to use it (e.g., export DISPLAY=localhost:0.0).
- Open OpenscadGDB.sln in VisualStudio
- Update the "openscad (Linux)" project's properties to reflect your build machine, directories, etc.
- Release/Debug is selected via VisualStudio
- Check build.sh for build options (devbuild, clang, precompiled headers, etc.)
- Release + devbuild is a pleasant debugging experience
- Press F5 to build and run
- Assuming it compiles and runs, it might not actually appear on screen:
  - Check Task Manager for two **openscad** processes on the Details tab, one of which is _suspended_
  - "End Task" on the _suspended_ process and debugging will continue
  - This is a problem with dbus on WSL which only manifests while debugging
  - I hear it is fixed in more recent versions of WSL
- Other than that one issue, debugging works beautifully
- I have provided a cpp.hint file to help VisualStudio's Intellisense parsing
- .props files are also provided to further aid VisualStudio's Intellisense parsing
- A full Release + devbuild on my cheap Core-I3 only takes 6 minutes using clang and pch!!!

