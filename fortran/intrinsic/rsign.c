double _rabs(double *x);

double _rsign(double *a, double *b) {
    return (*b >= 0.0) ?  _rabs(a) : -(_rabs(a));
}
