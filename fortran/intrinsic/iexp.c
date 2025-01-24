#include <math.h>

double _iexp(unsigned long waddr) {
    return exp((double)*((long *)(waddr << 3)));
}
