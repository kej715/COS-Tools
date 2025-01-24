#include <math.h>

double _iatan2(unsigned long waddra, unsigned long waddrb) {
    return atan2((double)*((long *)(waddra << 3)), (double)*((long *)(waddrb << 3)));
}
