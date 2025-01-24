#include <math.h>

double _rtanh(unsigned long waddr) {
    return tanh(*((double *)(waddr << 3)));
}
