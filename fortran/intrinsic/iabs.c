long _iabs(unsigned long waddr) {
    long x;
    x = *((long *)(waddr << 3));
    return (x < 0) ? -x : x;
}
