unsigned long _wloc(unsigned long ref) {
    return (ref & 0xffffffff) >> 3;
}
