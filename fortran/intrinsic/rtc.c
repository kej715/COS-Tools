extern long _irtc(void);

double _rtc(void) {
    return (double)(_irtc() & 0x3fffffffffff);
}
