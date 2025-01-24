#include <math.h>

double _rsinh(unsigned long waddr) {
    return sinh(*((double *)(waddr << 3)));
}
