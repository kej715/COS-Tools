#include <math.h>

double _iacos(unsigned long waddr) {
    return acos((double)*((long *)(waddr << 3)));
}
