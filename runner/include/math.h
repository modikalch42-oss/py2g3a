/* Complete-enough <math.h> for MicroPython on the Prizm.
 * libfxcg's own math.h only declares fabs(); the implementations come from
 * MicroPython's lib/libm_dbl (musl-derived) plus math_extra.c. */
#ifndef PY2G3A_MATH_H
#define PY2G3A_MATH_H

typedef float float_t;
typedef double double_t;

#define INFINITY  __builtin_inff()
#define NAN       __builtin_nanf("")
#define HUGE_VAL  __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4

int __fpclassifyd(double x);
int __signbitd(double x);

#define fpclassify(x) __fpclassifyd((double)(x))
#define isnan(x)      __builtin_isnan(x)
#define isinf(x)      __builtin_isinf(x)
#define isfinite(x)   __builtin_isfinite(x)
#define signbit(x)    __builtin_signbit(x)

#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_SQRT2    1.41421356237309504880

double fabs(double);
double sqrt(double);
double pow(double, double);
double exp(double);
double expm1(double);
double log(double);
double log2(double);
double log10(double);
double log1p(double);
double sin(double);
double cos(double);
double tan(double);
double asin(double);
double acos(double);
double atan(double);
double atan2(double, double);
double sinh(double);
double cosh(double);
double tanh(double);
double asinh(double);
double acosh(double);
double atanh(double);
double ceil(double);
double floor(double);
double trunc(double);
double round(double);
double rint(double);
double nearbyint(double);
double fmod(double, double);
double modf(double, double *);
double frexp(double, int *);
double ldexp(double, int);
double scalbn(double, int);
double copysign(double, double);
double erf(double);
double erfc(double);
double lgamma(double);
double tgamma(double);
double nan(const char *);

#endif
