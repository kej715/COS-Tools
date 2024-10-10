long _iabs(long *x);

long _isign(long *a, long *b) {
    return (*b >= 0) ?  _iabs(a) : -(_iabs(a));
}
