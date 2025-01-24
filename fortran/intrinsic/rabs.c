double _rabs(unsigned long waddr) {
    double x;

    x = *((double *)(waddr << 3));
    return (x < 0.0) ? -x : x;
}
