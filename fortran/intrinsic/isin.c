#include <math.h>

double _isin(unsigned long waddr) {
    return sin((double)*((long *)(waddr << 3)));
}
