#include <math.h>

double _rexp(unsigned long waddr) {
    return exp(*((double *)(waddr << 3)));
}
