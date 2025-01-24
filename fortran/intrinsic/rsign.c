double _rabs(unsigned long waddr);

double _rsign(unsigned long waddra, unsigned long waddrb) {
    return (*((double *)(waddrb << 3)) >= 0.0) ?  _rabs(waddra) : -(_rabs(waddra));
}
