#include <math.h>

double _ilog10(unsigned long waddr) {
    return log10((double)*((long *)(waddr << 3)));
}
