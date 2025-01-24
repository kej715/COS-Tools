#include <math.h>

double _icos(unsigned long waddr) {
    return cos((double)*((long *)(waddr << 3)));
}
