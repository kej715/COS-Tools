#include <math.h>

long _rint(unsigned long waddr) {
    double x;

    x = *((double *)(waddr << 3));
    return (x > 0) ? (long)floor(x) : (long)(-floor(-x));
}
