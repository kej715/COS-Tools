#include <math.h>

double _itan(unsigned long waddr) {
    return tan((double)*((long *)(waddr << 3)));
}
