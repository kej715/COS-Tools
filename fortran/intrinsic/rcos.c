#include <math.h>

double _rcos(unsigned long waddr) {
    return cos(*((double *)(waddr << 3)));
}
