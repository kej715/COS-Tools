double _ireal(unsigned long waddr) {
    return (double)(*((long *)(waddr << 3)));
}
