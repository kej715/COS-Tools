long _index(unsigned long s1ref, unsigned long s2ref) {
    int len;
    long res;
    char *s1;
    int s1len;
    char *s2;
    int s2len;
    char *sp1;
    char *sp2;

    s1 = (char *)(s1ref & 0xffffffff);
    s1len = s1ref >> 32;
    s2 = (char *)(s2ref & 0xffffffff);
    s2len = s2ref >> 32;
    res = 1;
    while (s1len >= s2len) {
        sp1 = s1;
        sp2 = s2;
        len = s2len;
        while (len > 0) {
            if (*sp1 != *sp2) break;
            sp1 += 1;
            sp2 += 1;
            len -= 1;
        }
        if (len <= 0) return res;
        s1 += 1;
        s1len -= 1;
        res += 1;
    }
    return 0;
}
