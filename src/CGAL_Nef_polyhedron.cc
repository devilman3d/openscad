#define FORCE_CGAL_PROFILE

#include "CGAL_Nef_polyhedron.h"
#include "cgal.h"
#include "cgalutils.h"
#include "printutils.h"
#include "polyset.h"
#include "svg.h"
#include "spinlock_pool_multi.h"

typedef CGAL::spinlock_pool_multi<2> NefPolyhedron_lock_pool;
typedef NefPolyhedron_lock_pool::scoped_lock NefPolyhedron_scoped_lock;

// CGAL_Nef_polyhedron3 <- CGAL::Nef_polyhedron3
CGAL_Nef_polyhedron3::CGAL_Nef_polyhedron3(const SNC_structure& W, const SNC_point_locator* _pl)
	: CGAL::Nef_polyhedron_3<CGAL_Kernel3>(EMPTY) {
	// clone snc
	{
		NefPolyhedron_scoped_lock lock(&W);
		snc() = W;
	}
	// clone pl
	SNC_point_locator* old_pl = pl();
	{
		NefPolyhedron_scoped_lock lock(_pl);
		pl() = _pl->clone();
	}
	set_snc(snc());
	pl()->initialize(&snc());
	delete old_pl;
	CGAL_assertion(this->unique() && "Created a non-unique cloned polyhedron");
}

CGAL_Nef_polyhedron3::CGAL_Nef_polyhedron3(Content space /*= Content::EMPTY*/)
	: CGAL::Nef_polyhedron_3<CGAL_Kernel3>(space) {
	CGAL_assertion(this->unique() && "Created a non-unique empty polyhedron");
}

CGAL_Nef_polyhedron3::CGAL_Nef_polyhedron3(const CGAL::Nef_polyhedron_3<CGAL_Kernel3> &p)
	: CGAL::Nef_polyhedron_3<CGAL_Kernel3>(p) {
	CGAL_assertion(!this->unique() && !p.unique() && "Created a unique polyhedron reference");
}

CGAL_Nef_polyhedron3::CGAL_Nef_polyhedron3(const CGAL_Nef_polyhedron3 &p)
	: CGAL_Nef_polyhedron3(p.snc(), p.pl()) {
	// TODO: isn't this already asserted in the private constructor???
	CGAL_assertion(this->unique() && "Created a non-unique copied polyhedron");
}

// CGAL_Nef_polyhedron

CGAL_Nef_polyhedron::CGAL_Nef_polyhedron()
{
	type = "Nef";
	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>();
	data.nef = this;
}

CGAL_Nef_polyhedron::CGAL_Nef_polyhedron(const shared_ptr<CGAL_Nef_polyhedron3> &p3)
{
	assert(p3 && "Creating a null Nef");
	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(*p3);
	type = "Nef: shared<p3>";
	data.nef = this;
}

CGAL_Nef_polyhedron::CGAL_Nef_polyhedron(const shared_ptr<CGAL_Nef_polyhedron3> &p3, const PolySet &ps)
{
	assert(p3 && "Creating a null Nef");
	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(*p3);
	type = "Nef: shared<p3> and PolySet";
	data = ps.data;
	data.nef = this;
}

CGAL_Nef_polyhedron::CGAL_Nef_polyhedron(const CGAL_Nef_polyhedron3 &p3)
{
	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(p3);
	type = "Nef: const &p3";
	data.nef = this;
}

CGAL_Nef_polyhedron::CGAL_Nef_polyhedron(const CGAL_Nef_polyhedron &src)
{
	assert(src.p3 && "Creating a null Nef");
	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(*src.p3);
	type = "Nef: const &src";
	data = src.data;
	data.nef = this;
}

//CGAL_Nef_polyhedron &CGAL_Nef_polyhedron::operator=(const CGAL_Nef_polyhedron &src)
//{
//	if (src.p3)
//		this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(*src.p3);
//	else
//		this->p3.reset();
//	data = src.data;
//	data.nef = this;
//	return *this;
//}

CGAL_Nef_polyhedron *CGAL_Nef_polyhedron::operator+(const CGAL_Nef_polyhedron &other) const
{
	CGAL_Nef_polyhedron3 tmp = this->p3->join(*other.p3);
	return new CGAL_Nef_polyhedron(tmp);
}

//CGAL_Nef_polyhedron& CGAL_Nef_polyhedron::operator+=(const CGAL_Nef_polyhedron &other)
//{
//	CGAL_Nef_polyhedron3 tmp = *this->p3 + *other.p3;
//	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(tmp);
//	data.reset(this);
//	return *this;
//}

CGAL_Nef_polyhedron *CGAL_Nef_polyhedron::intersection(const CGAL_Nef_polyhedron &other) const 
{
	CGAL_Nef_polyhedron3 tmp = this->p3->intersection(*other.p3);
	return new CGAL_Nef_polyhedron(tmp);
}

//CGAL_Nef_polyhedron& CGAL_Nef_polyhedron::operator*=(const CGAL_Nef_polyhedron &other)
//{
//	CGAL_Nef_polyhedron3 tmp = *this->p3 * *other.p3;
//	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(tmp);
//	data.reset(this);
//	return *this;
//}

CGAL_Nef_polyhedron *CGAL_Nef_polyhedron::difference(const CGAL_Nef_polyhedron &other) const 
{
	CGAL_Nef_polyhedron3 tmp = this->p3->difference(*other.p3);
	return new CGAL_Nef_polyhedron(tmp);
}

//CGAL_Nef_polyhedron& CGAL_Nef_polyhedron::operator-=(const CGAL_Nef_polyhedron &other)
//{
//	CGAL_Nef_polyhedron3 tmp = *this->p3 - *other.p3;
//	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(tmp);
//	data.reset(this);
//	return *this;
//}

CGAL_Nef_polyhedron *CGAL_Nef_polyhedron::minkowski(const CGAL_Nef_polyhedron &other) const 
{
	CGAL_Nef_polyhedron3 tmp = CGAL::minkowski_sum_3(*this->p3, *other.p3);
	return new CGAL_Nef_polyhedron(tmp);
}

//CGAL_Nef_polyhedron &CGAL_Nef_polyhedron::minkowski(const CGAL_Nef_polyhedron &other)
//{
//	CGAL_Nef_polyhedron3 tmp = CGAL::minkowski_sum_3(*this->p3, *other.p3);
//	this->p3 = std::make_shared<CGAL_Nef_polyhedron3>(tmp);
//	data.reset(this);
//	return *this;
//}

size_t CGAL_Nef_polyhedron::memsize() const
{
	if (this->isEmpty()) return 0;

	size_t memsize = sizeof(CGAL_Nef_polyhedron);
	memsize += this->p3->bytes();
	return memsize;
}

BoundingBox CGAL_Nef_polyhedron::getBoundingBox() const {
	if (this->isEmpty()) return BoundingBox();

	auto bb = CGALUtils::boundingBox(*p3);
	Vector3d min(bb.xmin().to_double(), bb.ymin().to_double(), bb.zmin().to_double());
	Vector3d max(bb.xmax().to_double(), bb.ymax().to_double(), bb.zmax().to_double());

	BoundingBox result(min, max);
	return result;
}

bool CGAL_Nef_polyhedron::isEmpty() const
{
	return !this->p3 || this->p3->is_empty();
}

void CGAL_Nef_polyhedron::resize(const Vector3d &newsize, 
                                 const Eigen::Matrix<bool,3,1> &autosize)
{
	// Based on resize() in Giles Bathgate's RapCAD (but not exactly)
	if (this->isEmpty()) return;

	CGAL_Iso_cuboid_3 bb = CGALUtils::boundingBox(*this->p3);

	std::vector<NT3> scale, bbox_size;
	for (unsigned int i=0;i<3;i++) {
		scale.push_back(NT3(1));
		bbox_size.push_back(bb.max_coord(i) - bb.min_coord(i));
	}
	int newsizemax_index = 0;
	for (unsigned int i=0;i<this->getDimension();i++) {
		if (newsize[i]) {
			if (bbox_size[i] == NT3(0)) {
				PRINT("WARNING: Resize in direction normal to flat object is not implemented");
				return;
			}
			else {
				scale[i] = NT3(newsize[i]) / bbox_size[i];
			}
			if (newsize[i] > newsize[newsizemax_index]) newsizemax_index = i;
		}
	}

	NT3 autoscale = NT3(1);
	if (newsize[newsizemax_index] != 0) {
		autoscale = NT3(newsize[newsizemax_index]) / bbox_size[newsizemax_index];
	}
	for (unsigned int i=0;i<this->getDimension();i++) {
		if (autosize[i] && newsize[i]==0) scale[i] = autoscale;
	}

	Eigen::Matrix4d t;
	t << CGAL::to_double(scale[0]),           0,        0,        0,
	     0,        CGAL::to_double(scale[1]),           0,        0,
	     0,        0,        CGAL::to_double(scale[2]),           0,
	     0,        0,        0,                                   1;

	this->transform(Transform3d(t));
}

std::string CGAL_Nef_polyhedron::dump() const
{
	return OpenSCAD::dump_svg( *this->p3 );
}

void CGAL_Nef_polyhedron::transform( const Transform3d &matrix )
{
	if (!this->isEmpty()) {
		if (matrix.matrix().determinant() == 0) {
			PRINT("WARNING: Scaling a 3D object with 0 - removing object");
			this->reset();
		}
		else {
			CGAL_Aff_transformation t(
				matrix(0,0), matrix(0,1), matrix(0,2), matrix(0,3),
				matrix(1,0), matrix(1,1), matrix(1,2), matrix(1,3),
				matrix(2,0), matrix(2,1), matrix(2,2), matrix(2,3), matrix(3,3));
			p3->transform(t);
			data.reset(this);
		}
	}
}
