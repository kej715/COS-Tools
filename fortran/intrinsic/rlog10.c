#include <math.h>

double _rlog10(unsigned long waddr) {
    return log10(*((double *)(waddr << 3)));
}
