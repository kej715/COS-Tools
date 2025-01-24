double _aint(unsigned long waddr) {
    return (double)((long)*((double *)(waddr << 3)));
}
