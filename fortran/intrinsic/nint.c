long _nint(unsigned long waddr) {
    long res;
    double x;

    x = *((double *)(waddr << 3));
    if (x > 0)
        res = (long)(x + 0.5);
    else if (x < 0)
        res = (long)(x - 0.5);
    else
        res = (long)x;
    return res;
}
