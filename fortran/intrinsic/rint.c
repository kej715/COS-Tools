#include <math.h>

double _rint(double *x) {

return (*x > 0) ? floor(*x) : -floor(-*x);
}
