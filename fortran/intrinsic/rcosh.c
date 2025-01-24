#include <math.h>

double _rcosh(unsigned long waddr) {
    return cosh(*((double *)(waddr << 3)));
}
