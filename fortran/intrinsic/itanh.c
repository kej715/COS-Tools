#include <math.h>

double _itanh(unsigned long waddr) {
    return tanh((double)*((long *)(waddr << 3)));
}
