#include <stdarg.h>

long _imax1(unsigned long waddr, ...) {
    va_list ap;
    long count;
    double item;
    double res;

    count = *((long *)(waddr << 3));
    va_start(ap, waddr);
    res = *((double *)(va_arg(ap, unsigned long) << 3));
    while (--count > 0) {
        item = *((double *)(va_arg(ap, unsigned long) << 3));
        if (item > res) res = item;
    }
    va_end(ap);

    return (long)res;
}
