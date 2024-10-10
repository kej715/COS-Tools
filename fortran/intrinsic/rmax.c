#include <stdarg.h>

double _rmax(int count, ...) {
    va_list ap;
    double item;
    double res;

    va_start(ap, count);
    res = va_arg(ap, double);
    while (--count > 0) {
        item = va_arg(ap, double);
        if (item > res) res = item;
    }
    va_end(ap);

    return res;
}
