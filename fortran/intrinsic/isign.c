long _iabs(unsigned long waddr);

long _isign(unsigned long waddra, unsigned long waddrb) {
    return (*((double *)(waddrb << 3)) >= 0) ? _iabs(waddra) : -(_iabs(waddra));
}
