double _anint(unsigned long waddr) {
    double res;
    double x;

    x = *((double *)(waddr << 3));
    if (x > 0)
        res = (double)((long)(x + 0.5));
    else if (x < 0)
        res = (double)((long)(x - 0.5));
    else
        res = x;
    return res;
}
