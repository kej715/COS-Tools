#include <math.h>

double _rasin(unsigned long waddr) {
    return asin(*((double *)(waddr << 3)));
}
