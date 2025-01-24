#include <math.h>

double _isinh(unsigned long waddr) {
    return sinh((double)*((long *)(waddr << 3)));
}
