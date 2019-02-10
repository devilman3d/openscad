#pragma once

#include "renderer.h"
#include "csgnode.h"
#include <unordered_map>
#include <boost/functional/hash.hpp>

class CSGProducts;
class CSGChainObject;

class ThrownTogetherRenderer : public Renderer
{
public:
	ThrownTogetherRenderer(shared_ptr<CSGProducts> root_products,
		shared_ptr<CSGProducts> highlight_products,
		shared_ptr<CSGProducts> background_products);
	virtual void draw(bool showfaces, bool showedges) const;
	virtual BoundingBox getBoundingBox() const;
private:
	void renderCSGProducts(const CSGProducts &products, bool highlight_mode, bool background_mode, bool showedges, bool fberror) const;
	void renderChainObject(const CSGChainObject &csgobj, bool highlight_mode, bool background_mode, 
		bool showedges, bool fberror, OpenSCADOperator type) const;

	shared_ptr<CSGProducts> root_products;
	shared_ptr<CSGProducts> highlight_products;
	shared_ptr<CSGProducts> background_products;

	typedef std::pair<const Geometry*, const Transform3d*> GeomWithTransform;
	typedef boost::hash<GeomWithTransform> GeomWithTransformHash;

	mutable std::unordered_map<GeomWithTransform, int, GeomWithTransformHash> geomVisitMark;
};
