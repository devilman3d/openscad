#pragma once

#include "system-gl.h"
#include "linalg.h"
#include "memory.h"
#include "colormap.h"

#ifdef _MSC_VER // NULL
#include <cstdlib>
#endif

class Renderer
{
public:
	Renderer();
	virtual ~Renderer() {}
	virtual void draw(bool showfaces, bool showedges) const = 0;
	virtual BoundingBox getBoundingBox() const = 0;
	
#define CSGMODE_DIFFERENCE_FLAG 0x10
	enum csgmode_e {
		CSGMODE_NONE                  = 0x00,
		CSGMODE_NORMAL                = 0x01,
		CSGMODE_DIFFERENCE            = CSGMODE_NORMAL | CSGMODE_DIFFERENCE_FLAG,
		CSGMODE_BACKGROUND            = 0x02,
		CSGMODE_BACKGROUND_DIFFERENCE = CSGMODE_BACKGROUND | CSGMODE_DIFFERENCE_FLAG,
		CSGMODE_HIGHLIGHT             = 0x03,
		CSGMODE_HIGHLIGHT_DIFFERENCE  = CSGMODE_HIGHLIGHT | CSGMODE_DIFFERENCE_FLAG
	};

	enum ColorMode {
		COLORMODE_NONE,
		COLORMODE_MATERIAL,
		COLORMODE_CUTOUT,
		COLORMODE_HIGHLIGHT,
		COLORMODE_BACKGROUND,
		COLORMODE_MATERIAL_EDGES,
		COLORMODE_CUTOUT_EDGES,
		COLORMODE_HIGHLIGHT_EDGES,
		COLORMODE_BACKGROUND_EDGES,
		COLORMODE_EMPTY_SPACE
	};

	static constexpr int ColorModeCount = (ColorMode::COLORMODE_EMPTY_SPACE - ColorMode::COLORMODE_NONE) + 1;

	virtual bool getColor(ColorMode colormode, Color4f &col) const;
	virtual void setColor(const float color[4]) const;
	virtual void setColor(ColorMode colormode) const;
	virtual void setColor(ColorMode colormode, const float color[4]) const;
	virtual void setColor(ColorMode colormode, const Color4f &col) const;
	virtual void setColorScheme(const ColorScheme &cs);

	static void render_surface(shared_ptr<const class Geometry> geom, csgmode_e csgmode, bool mirrored);
	static void render_edges(shared_ptr<const Geometry> geom, csgmode_e csgmode);

protected:
	Color4f colormap[ColorModeCount];
	const ColorScheme *colorscheme;
};
