#include <math.h>

double _rsin(unsigned long waddr) {
    return sin(*((double *)(waddr << 3)));
}
