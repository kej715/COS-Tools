#include <math.h>

double _icosh(unsigned long waddr) {
    return cosh((double)*((long *)(waddr << 3)));
}
