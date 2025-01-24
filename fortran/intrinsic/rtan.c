#include <math.h>

double _rtan(unsigned long waddr) {
    return tan(*((double *)(waddr << 3)));
}
