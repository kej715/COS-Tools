#include <math.h>

double _iatan(unsigned long waddr) {
    return atan((double)*((long *)(waddr << 3)));
}
