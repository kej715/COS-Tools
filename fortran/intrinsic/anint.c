double _anint(double *x) {
    double res;

    if (*x > 0)
        res = (double)((long)(*x + 0.5));
    else if (*x < 0)
        res = (double)((long)(*x - 0.5));
    else
        res = *x;
    return res;
}
