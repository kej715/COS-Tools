#include <stdarg.h>

double _amin0(unsigned long waddr, ...) {
    va_list ap;
    long count;
    long item;
    long res;

    count = *((long *)(waddr << 3));
    va_start(ap, waddr);
    res = *((long *)(va_arg(ap, unsigned long)));
    while (--count > 0) {
        item = *((long *)(va_arg(ap, unsigned long)));
        if (item < res) res = item;
    }
    va_end(ap);

    return (double)res;
}
