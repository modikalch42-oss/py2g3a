// The two maths functions MicroPython needs that lib/libm_dbl doesn't provide.
#include <math.h>

double log2(double x) {
    return log(x) * M_LOG2E;
}

double nan(const char *tag) {
    (void)tag;
    return __builtin_nan("");
}
