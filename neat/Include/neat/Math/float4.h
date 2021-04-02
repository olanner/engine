
#pragma once

struct float4
{
	float x = 0;
	float y = 0;
	float z = 0;
	float w = 0;

	void operator+=( const float4& b )
	{
		x += b.x;
		y += b.y;
		z += b.z;
		w += b.w;
	}
	void operator-=( const float4& b )
	{
		x -= b.x;
		y -= b.y;
		z -= b.z;
		w -= b.w;
	}
	void operator*=( const float4& b )
	{
		x *= b.x;
		y *= b.y;
		z *= b.z;
		w *= b.w;
	}
	void operator/=( const float4& b )
	{
		x /= b.x;
		y /= b.y;
		z /= b.z;
		w /= b.w;
	}
	void operator+=( const float& b )
	{
		x += b;
		y += b;
		z += b;
		w += b;
	}
	void operator-=( const float& b )
	{
		x -= b;
		y -= b;
		z -= b;
		w -= b;
	}
	void operator*=( const float& b )
	{
		x *= b;
		y *= b;
		z *= b;
		w *= b;
	}
	void operator/=( const float& b )
	{
		x /= b;
		y /= b;
		z /= b;
		w /= b;
	}

};

// PER COMPONENT MATH
inline float4 operator+( const float4& a, const float4& b )
{
	return {
		a.x + b.x,
		a.y + b.y,
		a.z + b.z,
		a.w + b.w };
}

inline float4 operator-( const float4& a, const float4& b )
{
	return {
		a.x - b.x,
		a.y - b.y,
		a.z - b.z,
		a.w - b.w };
}

inline float4 operator*( const float4& a, const float4& b )
{
	return {
		a.x * b.x,
		a.y * b.y,
		a.z * b.z,
		a.w * b.w };
}

inline float4 operator/( const float4& a, const float4& b )
{
	return {
		a.x / b.x,
		a.y / b.y,
		a.z / b.z,
		a.w / b.w };
}

// PER COMPONENT SCALAR MATH
inline float4 operator+( const float4& a, const float& b )
{
	return {
		a.x + b,
		a.y + b,
		a.z + b,
		a.w + b };
}
inline float4 operator+( const float& a, const float4& b )
{
	return {
		a + b.x,
		a + b.y,
		a + b.z,
		a + b.w };
}

inline float4 operator-( const float4& a, const float& b )
{
	return {
		a.x - b,
		a.y - b,
		a.z - b,
		a.w - b };
}
inline float4 operator-( const float& a, const float4& b )
{
	return {
		a - b.x,
		a - b.y,
		a - b.z,
		a - b.w };
}

inline float4 operator*( const float4& a, const float& b )
{
	return {
		a.x * b,
		a.y * b,
		a.z * b,
		a.w * b };
}
inline float4 operator*( const float& a, const float4& b )
{
	return {
		a * b.x,
		a * b.y,
		a * b.z,
		a * b.w };
}

inline float4 operator/( const float4& a, const float& b )
{
	return {
		a.x / b,
		a.y / b,
		a.z / b,
		a.w / b };
}
inline float4 operator/( const float& a, const float4& b )
{
	return {
		a / b.x,
		a / b.y,
		a / b.z,
		a / b.w };
}

// VECTOR MATH
inline float DOT( const float4& a, const float4& b )
{
	return
		a.x * b.x +
		a.y * b.y +
		a.z * b.z +
		a.w * a.w;
}
inline float4 CROSS( const float4& a, const float4& b )
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.w - a.w * b.z,
		a.w * b.x - a.x * b.w,
		a.x * b.y - a.y * b.x };
}
