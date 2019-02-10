#include "Geometry.h"
#include "node.h"
#include "Geometry.h"
#include "Polygon2d.h"

#include "cgalutils.h"
#include "clipper-utils.h"

#include "maybe_const.h"

namespace GeomUtils
{
	template<typename Base, typename T>
	using _Derived = typename std::enable_if<
		std::is_base_of<Base, T>::value
	>::type;

	// cast shared_ptr -> shared_ptr
	template <typename Out, typename In, typename = _Derived<In, Out>>
	shared_ptr<Out> _convert(const shared_ptr<In> &src) { return dynamic_pointer_cast<Out>(src); }

	// cast pointer -> pointer
	template <typename Out, typename In, typename = _Derived<In, Out>>
	Out *_convertPointer(In *src) { return dynamic_cast<Out*>(src); }

	// cast shared_ptr -> ptr
	template <typename Out, typename In, typename = _Derived<In, Out>>
	Out *_convertPointer(const shared_ptr<In> &src) { return dynamic_pointer_cast<Out>(src).get(); }

	GeometryHandle _apply(const GeometryHandles &flat, OpenSCADOperator op, int dim)
	{
		if (dim == 2) {
			Polygon2ds pp;
			collect(flat, pp);
			ClipperLib::ClipType ct =
				op == OPENSCAD_DIFFERENCE ? ClipperLib::ClipType::ctDifference :
				op == OPENSCAD_INTERSECTION ? ClipperLib::ClipType::ctIntersection :
				ClipperLib::ClipType::ctUnion;
			ClipperUtils utils;
			return GeometryHandle(utils.apply(pp, ct));
		}
		if (dim == 3)
			return GeometryHandle(CGALUtils::applyOperator(flat, op));
		return nullptr;
	}

	ResultObject apply(const NodeGeometries &src, OpenSCADOperator op, int dim /* = 0 */)
	{
		GeometryHandles flat;
		collect(src, flat, dim, op == OPENSCAD_DIFFERENCE, op == OPENSCAD_INTERSECTION);
		return ResultObject(_apply(flat, op, dim));
	}

	ResultObject apply(const GeometryHandles &src, OpenSCADOperator op, int dim /* = 0 */)
	{
		GeometryHandles flat;
		collect(src, flat, dim, op == OPENSCAD_DIFFERENCE, op == OPENSCAD_INTERSECTION);
		return ResultObject(_apply(flat, op, dim));
	}

	GeometryHandle _checkChild(const NodeGeometry &child)
	{
		if (child.first->isBackground())
			return nullptr;
		if (!child.second)
			return nullptr;
		if (child.second->isEmpty())
			return nullptr;
		return child.second;
	}

	GeometryHandle _checkChild(const GeometryHandle &child)
	{
		if (!child)
			return nullptr;
		if (child->isEmpty())
			return nullptr;
		return child;
	}

	const Geometry *_checkChild(const Geometry *child)
	{
		if (!child)
			return nullptr;
		if (child->isEmpty())
			return nullptr;
		return child;
	}

	const GeometryGroup *_getGroup(const GeometryHandle &child)
	{
		return dynamic_pointer_cast<const GeometryGroup>(child).get();
	}

	const GeometryGroup *_getGroup(const Geometry *child)
	{
		return dynamic_cast<const GeometryGroup*>(child);
	}

	template <typename T, typename U>
	void _collect(const std::vector<T> &src, std::vector<shared_ptr<U>> &dest, int &dim, bool unionFirst = false, bool unionGroups = false)
	{
		for (auto &child : src) {
			if (auto t = _checkChild(child)) {
				if (auto gg = _getGroup(t)) {
					bool doUnion = unionGroups || (unionFirst && dest.empty());
					GeometryHandles flat;
					_collect(gg->getChildren(), flat, dim);
					if (doUnion) {
						if (auto unioned = _apply(flat, OPENSCAD_UNION, dim))
							if (auto pp = _convert<U>(unioned))
								dest.push_back(pp);
					}
					else {
						for (auto ff : flat)
							if (auto pp = _convert<U>(ff))
								dest.push_back(pp);
					}
				}
				else {
					if (dim == 0)
						dim = t->getDimension();
					if (dim == t->getDimension()) {
						if (auto pp = _convert<U>(t))
							dest.push_back(pp);
					}
					else
						PRINT("WARNING: Mixing 2D and 3D objects is not supported.");
				}
			}
		}
	}

	template <typename T, typename U>
	void _collectPointers(const std::vector<T> &src, std::vector<U*> &dest, int &dim)
	{
		for (auto &child : src) {
			if (auto t = _checkChild(child)) {
				if (auto gg = _getGroup(t)) {
					_collectPointers(gg->getChildren(), dest, dim);
				}
				else {
					if (dim == 0)
						dim = t->getDimension();
					if (dim == t->getDimension()) {
						if (auto pp = _convertPointer<U>(t))
							dest.push_back(pp);
					}
					else
						PRINT("WARNING: Mixing 2D and 3D objects is not supported.");
				}
			}
		}
	}

	void collect(const NodeGeometries &src, GeometryHandles &dest, int &dim, bool unionFirst /*= false*/, bool unionGroups /*= false*/)
	{
		_collect(src, dest, dim, unionFirst, unionGroups);
	}

	void collect(const GeometryHandles &src, GeometryHandles &dest, int &dim, bool unionFirst /*= false*/, bool unionGroups /*= false*/)
	{
		_collect(src, dest, dim, unionFirst, unionGroups);
	}

	void collect(const NodeGeometries &src, Polygon2dHandles &dest, bool unionFirst /*= false*/, bool unionGroups /*= false*/)
	{
		int dim = 2;
		_collect(src, dest, dim, unionFirst, unionGroups);
	}

	void collect(const GeometryHandles &src, Polygon2dHandles &dest, bool unionFirst /*= false*/, bool unionGroups /*= false*/)
	{
		int dim = 2;
		_collect(src, dest, dim, unionFirst, unionGroups);
	}

	void collect(const NodeGeometries &src, Geometries &dest, int &dim)
	{
		_collectPointers(src, dest, dim);
	}

	void collect(const GeometryHandles &src, Geometries &dest, int &dim)
	{
		_collectPointers(src, dest, dim);
	}

	void collect(const NodeGeometries &src, Polygon2ds &dest)
	{
		int dim = 2;
		_collectPointers(src, dest, dim);
	}

	void collect(const GeometryHandles &src, Polygon2ds &dest)
	{
		int dim = 2;
		_collectPointers(src, dest, dim);
	}

	void collect(const Geometries &src, Polygon2ds &dest)
	{
		int dim = 2;
		_collectPointers(src, dest, dim);
	}

	GeometryHandle simplify(const GeometryHandle &g)
	{
		if (g) {
			if (auto group = dynamic_pointer_cast<const GeometryGroup>(g)) {
				NodeGeometries temp;
				for (auto &gc : group->getChildren())
					if (auto sgc = simplify(gc.second))
						temp.push_back(NodeGeometry(gc.first, sgc));
				if (temp.empty())
					return nullptr;
				if (temp.size() == 1)
					return temp.front().second;
				return GeometryHandle(new GeometryGroup(temp));
			}
		}
		return g;
	}

	ResultObject simplify(const ResultObject &res)
	{
		if (auto result = simplify(res.constptr()))
			return ResultObject(result);
		return ResultObject(new EmptyGeometry());
	}

	bool splitfirst(const Vector3d &pp0, const Vector3d &pp1, const Vector3d &pp2, const Vector3d &pp3)
	{
		CGAL_Kernel3::Point_3 p0(pp0[0], pp0[1], pp0[2]);
		CGAL_Kernel3::Point_3 p1(pp1[0], pp1[1], pp1[2]);
		CGAL_Kernel3::Point_3 p2(pp2[0], pp2[1], pp2[2]);
		CGAL_Kernel3::Point_3 p3(pp3[0], pp3[1], pp3[2]);

		CGAL_Kernel3::FT p1p3 = CGAL::cross_product(p2 - p1, p3 - p2) * CGAL::cross_product(p0 - p3, p1 - p0);
		CGAL_Kernel3::FT p0p2 = CGAL::cross_product(p1 - p0, p1 - p2) * CGAL::cross_product(p3 - p2, p3 - p0);

		return (p0p2 > p1p3);
	}
}
