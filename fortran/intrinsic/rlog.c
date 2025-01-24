#include <math.h>

double _rlog(unsigned long waddr) {
    return log(*((double *)(waddr << 3)));
}
