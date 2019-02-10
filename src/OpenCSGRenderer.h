#pragma once

#include "renderer.h"
#include "system-gl.h"
#ifdef ENABLE_OPENCSG
#include <opencsg.h>
#endif
#include "csgnode.h"

class OpenCSGRenderer : public Renderer
{
public:
	OpenCSGRenderer(shared_ptr<class CSGProducts> root_products,
					shared_ptr<CSGProducts> highlights_products,
					shared_ptr<CSGProducts> background_products);
	virtual void draw(bool showfaces, bool showedges) const;
	virtual BoundingBox getBoundingBox() const;
private:
	void renderCSGProducts(const class CSGProducts &products, bool showedges, bool highlight_mode, bool background_mode) const;

	shared_ptr<CSGProducts> root_products;
	shared_ptr<CSGProducts> highlights_products;
	shared_ptr<CSGProducts> background_products;
};
