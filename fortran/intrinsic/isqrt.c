#include <math.h>

double _isqrt(unsigned long waddr) {
    return sqrt((double)*((long *)(waddr << 3)));
}
