#include <math.h>

double _ilog(unsigned long waddr) {
    return log((double)*((long *)(waddr << 3)));
}
