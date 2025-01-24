#include <math.h>

double _rsqrt(unsigned long waddr) {
    return sqrt(*((double *)(waddr << 3)));
}
