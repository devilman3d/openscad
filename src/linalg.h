#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Dense>
#include <cstdint>

using Eigen::Vector2d;
using Eigen::Vector3d;
using Eigen::Vector3f;
using Eigen::Vector3i;
using Eigen::Vector4d;
using Eigen::Vector4f;

using Eigen::Matrix3f;
using Eigen::Matrix3d;
using Eigen::Matrix4d;

typedef Eigen::AlignedBox<double, 3> BoundingBox;

typedef Eigen::Affine3d Transform3d;
typedef Eigen::Affine2d Transform2d;

bool matrix_contains_infinity( const Transform3d &m );
bool matrix_contains_nan( const Transform3d &m );
int32_t hash_floating_point( double v );

// from C++17: return the size of an array
template <class T, std::size_t N>
constexpr std::size_t size(const T(&array)[N]) noexcept
{
	return N;
}

template<typename Derived> bool is_finite(const Eigen::MatrixBase<Derived>& x) {
   return ((x - x).array() == (x - x).array()).all();
}

template<typename Derived> bool is_nan(const Eigen::MatrixBase<Derived>& x) {
   return !((x.array() == x.array())).all();
}

BoundingBox operator*(const Transform3d &m, const BoundingBox &box);

class Color4f : public Vector4f
{
public:
	Color4f() { }
	Color4f(int r, int g, int b, int a = 255) { setRgb(r,g,b,a); }
	Color4f(float r, float g, float b, float a = 1.0f) : Vector4f(r, g, b, a) { }
	Color4f(double r, double g, double b, double a = 1.0) : Vector4f((float)r, (float)g, (float)b, (float)a) { }

	void setRgb(int r, int g, int b, int a = 255) {
		*this << r/255.0f, g/255.0f, b/255.0f, a/255.0f;
	}

	bool isValid() const { return this->minCoeff() >= 0.0f; }
};
