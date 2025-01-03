#include <stdarg.h>

long _imax(int count, ...) {
    va_list ap;
    long item;
    long res;

    va_start(ap, count);
    res = *va_arg(ap, long *);
    while (--count > 0) {
        item = *va_arg(ap, long *);
        if (item > res) res = item;
    }
    va_end(ap);

    return res;
}
