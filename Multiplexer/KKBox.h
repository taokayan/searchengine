
#pragma once

#include "KKBase.h"

#include <immintrin.h>

#define KK_SSE
//#define KK_AVX

#ifdef KK_AVX
typedef __m256 KKM256;
#else
typedef struct _CRT_ALIGN(32) { 
    float m256_f32[8];
} KKM256;
#endif

#define KKSIMD2Op4(a,b,c) \
	a[0] b c[0]; \
	a[1] b c[1]; \
	a[2] b c[2]; \
	a[3] b c[3]

#define KKSIMD3Op4(a,b,c,d) \
	a[0] = b[0] c d[0]; \
	a[1] = b[1] c d[1]; \
	a[2] = b[2] c d[2]; \
	a[3] = b[3] c d[3]

#define KKSIMD2Op8(a,b,c) \
	a[0] b c[0]; \
	a[1] b c[1]; \
	a[2] b c[2]; \
	a[3] b c[3]; \
	a[4] b c[4]; \
	a[5] b c[5]; \
	a[6] b c[6]; \
	a[7] b c[7]

#define KKSIMD3Op8(a,b,c,d) \
	a[0] = b[0] c d[0]; \
	a[1] = b[1] c d[1]; \
	a[2] = b[2] c d[2]; \
	a[3] = b[3] c d[3]; \
	a[4] = b[4] c d[4]; \
	a[5] = b[5] c d[5]; \
	a[6] = b[6] c d[6]; \
	a[7] = b[7] c d[7]


template <typename T> struct KKSIMD256;

template <>
struct KKSIMD256<float>
{
	KKM256 m_v;
	
	inline KKSIMD256() { }
	inline KKSIMD256(const KKSIMD256 &v) : m_v(v.m_v) { }
	inline KKSIMD256(float s) {
#ifdef KK_AVX
		m_v = _mm256_broadcast_ss(&s);
#else 
		m_v.m256_f32[0] = s;
		m_v.m256_f32[1] = s;
		m_v.m256_f32[2] = s;
		m_v.m256_f32[3] = s;
		m_v.m256_f32[4] = s;
		m_v.m256_f32[5] = s;
		m_v.m256_f32[6] = s;
		m_v.m256_f32[7] = s;
#endif
	}
	inline KKSIMD256(float *v) {
		KKSIMD2Op8(m_v.m256_f32, =, v);
	}

	inline KKSIMD256 &operator =(const KKSIMD256<float> &v) {
		m_v = v.m_v;
		return *this;
	}
	inline KKSIMD256 &operator =(float s) {
#ifdef KK_AVX
		m_v = _mm256_broadcast_ss(&s);
#else 
		m_v.m256_f32[0] = s;
		m_v.m256_f32[1] = s;
		m_v.m256_f32[2] = s;
		m_v.m256_f32[3] = s;
		m_v.m256_f32[4] = s;
		m_v.m256_f32[5] = s;
		m_v.m256_f32[6] = s;
		m_v.m256_f32[7] = s;
#endif
		return *this;
	}

	inline KKSIMD256 operator +(const KKSIMD256 &v) {
		KKSIMD256 c;
#ifdef KK_AVX
		c.m_v = _mm256_add_ps(m_v, v.m_v);
#else 
		KKSIMD3Op8(c.m_v.m256_f32, m_v.m256_f32, +, v.m_v.m256_f32);
#endif
		return c;
	}
	inline KKSIMD256 operator -(const KKSIMD256 &v) {
		KKSIMD256 c;
#ifdef KK_AVX
		c.m_v = _mm256_sub_ps(m_v, v.m_v);
#else 
		KKSIMD3Op8(c.m_v.m256_f32, m_v.m256_f32, -, v.m_v.m256_f32);
#endif
		return c;
	}
	inline KKSIMD256 operator *(const KKSIMD256 &v) {
		KKSIMD256 c;
#ifdef KK_AVX
		c.m_v = _mm256_mul_ps(m_v, v.m_v);
#else
		KKSIMD3Op8(c.m_v.m256_f32, m_v.m256_f32, *, v.m_v.m256_f32);
#endif
		return c;
	}
	inline KKSIMD256 operator /(const KKSIMD256 &v) {
		KKSIMD256 c;
#ifdef KK_AVX
		c.m_v = _mm256_div_ps(m_v, v.m_v);
#else
		KKSIMD3Op8(c.m_v.m256_f32, m_v.m256_f32, /, v.m_v.m256_f32);
#endif
		return c;
	}
	inline float &operator [](int i) {
		return m_v.m256_f32[i];
	}
	inline const float &operator [](int i) const {
		return m_v.m256_f32[i];
	}
};


#ifdef KK_AVX
typedef __m256d KKM256d;
#else
typedef struct _CRT_ALIGN(32) { 
    double m256d_f64[4];
} KKM256d;
#endif

template <>
struct KKSIMD256<double>
{
	KKM256d m_v;
	
	inline KKSIMD256() { }
	inline KKSIMD256(const KKSIMD256 &v) : m_v(v.m_v) { }
	inline KKSIMD256(double s) {
#ifdef KK_AVX
		m_v = _mm256_broadcast_sd(&s);
#else 
		m_v.m256d_f64[0] = s;
		m_v.m256d_f64[1] = s;
		m_v.m256d_f64[2] = s;
		m_v.m256d_f64[3] = s;
#endif
	}
	inline KKSIMD256(double *v) {
		KKSIMD2Op4(m_v.m256d_f64, =, v);
	}

	inline KKSIMD256 &operator =(const KKSIMD256<double> &v) {
		m_v = v.m_v;
		return *this;
	}
	inline KKSIMD256 &operator =(double s) {
#ifdef KK_AVX
		m_v = _mm256_broadcast_sd(&s);
#else 
		m_v.m256d_f64[0] = s;
		m_v.m256d_f64[1] = s;
		m_v.m256d_f64[2] = s;
		m_v.m256d_f64[3] = s;
#endif
		return *this;
	}
	inline void set(double v0, double v1, double v2, double v3) {
//#ifdef KK_AVX
//		m_v = _mm256_set_pd(v0, v1, v2, v3); // note: slow!!!
//#else
		m_v.m256d_f64[0] = v0;
		m_v.m256d_f64[1] = v1;
		m_v.m256d_f64[2] = v2;
		m_v.m256d_f64[3] = v3;
//#endif
	}

	inline KKSIMD256 operator +(const KKSIMD256 &v) {
		KKSIMD256 c;
#ifdef KK_AVX
		c.m_v = _mm256_add_pd(m_v, v.m_v);
#else 
		KKSIMD3Op4(c.m_v.m256d_f64, m_v.m256d_f64, +, v.m_v.m256d_f64);
#endif
		return c;
	}
	inline KKSIMD256 operator -(const KKSIMD256 &v) {
		KKSIMD256 c;
#ifdef KK_AVX
		c.m_v = _mm256_sub_pd(m_v, v.m_v);
#else 
		KKSIMD3Op4(c.m_v.m256d_f64, m_v.m256d_f64, -, v.m_v.m256d_f64);
#endif
		return c;
	}
	inline KKSIMD256 operator *(const KKSIMD256 &v) {
		KKSIMD256 c;
#ifdef KK_AVX
		c.m_v = _mm256_mul_pd(m_v, v.m_v);
#else
		KKSIMD3Op4(c.m_v.m256d_f64, m_v.m256d_f64, *, v.m_v.m256d_f64);
#endif
		return c;
	}
	inline KKSIMD256 operator /(const KKSIMD256 &v) {
		KKSIMD256 c;
#ifdef KK_AVX
		c.m_v = _mm256_div_pd(m_v, v.m_v);
#else
		KKSIMD3Op8(c.m_v.m256d_f64, m_v.m256d_f64, /, v.m_v.m256d_f64);
#endif
		return c;
	}
	inline double &operator [](int i) {
		return m_v.m256d_f64[i];
	}
	inline const double &operator [](int i) const {
		return m_v.m256d_f64[i];
	}
};



