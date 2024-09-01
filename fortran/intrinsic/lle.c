int _cmpstr(unsigned long s1, unsigned long s2);

long _lle(unsigned long s1, unsigned long s2) {
    return _cmpstr(s1, s2) <= 0 ? ~0L : 0;
}
