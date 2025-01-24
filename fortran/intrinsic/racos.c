#include <math.h>

double _racos(unsigned long waddr) {
    return acos(*((double *)(waddr << 3)));
}
