#include <math.h>

double _ratan(unsigned long waddr) {
    return atan(*((double *)(waddr << 3)));
}
