#include <math.h>

long _rint(double *x) {

return (*x > 0) ? (long)floor(*x) : (long)(-floor(-*x));
}
