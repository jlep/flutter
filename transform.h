#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <opencv2/opencv.hpp>
#include <iostream>
#include <cmath>

template <typename T>
struct Transform {
	T x;
	T y;
	T a;
	Transform() : x(0.0), y(0.0), a(0.0) {}
	Transform(T x, T y, T a) : x(x), y(y), a(a) {}
	// convert affine transformation matrix
	Transform(const cv::Mat& m):
		x(m.at<T>(0,2)),
		y(m.at<T>(1,2)),
		a(atan2(m.at<T>(1,0), m.at<T>(0.0)))
	{
	}
	Transform& operator+=(const Transform& t)
	{
		x += t.x;
		y += t.y;
		a += t.a;
		return *this;
	}
	Transform operator+(const Transform& t) const
	{
		return Transform(x + t.x, y + t.y, a + t.a);
	}
	Transform operator-() const
	{
		return Transform(-x, -y, -a);
	}
	Transform operator-(const Transform& t) const
	{
		return *this + -t;
	}
	Transform operator*(T c) const
	{
		return Transform(c*x, c*y, c*a);
	}
	cv::Mat toMat() const
	{
		return cv::Mat_<T>(2,3) <<
			cos(a), -sin(a), x,
			sin(a),  cos(a), y;
	}
	cv::Mat toVec() const
	{
		return cv::Mat_<T>(3,1) << x, y, a;
	}
	T abs() const
	{
		return sqrt(x*x+y*y+a*a);
	}
	static Transform fromVec(const cv::Mat& m)
	{
		return Transform(m.at<T>(0),m.at<T>(1),m.at<T>(2));
	}
};

template <typename T>
Transform<T> operator*(T c, const Transform<T>& t)
{
	return t*c;
}

template <typename T>
std::ostream& operator<<(std::ostream& o, const Transform<T>& t)
{
	return o << "{ x: " << t.x << ", y: " << t.y << ", a: " << t.a << " }";
}

#endif // TRANSFORM_H