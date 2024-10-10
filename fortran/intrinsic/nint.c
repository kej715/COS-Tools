long _nint(double *x) {
    long res;

    if (*x > 0)
        res = (long)(*x + 0.5);
    else if (*x < 0)
        res = (long)(*x - 0.5);
    else
        res = (long)*x;
    return res;
}
