#include <math.h>

double _iasin(unsigned long waddr) {
    return asin((double)*((long *)(waddr << 3)));
}
