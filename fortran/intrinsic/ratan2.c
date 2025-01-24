#include <math.h>

double _ratan2(unsigned long waddra, unsigned long waddrb) {
    return atan2(*((double *)(waddra << 3)), *((double *)(waddrb << 3)));
}
