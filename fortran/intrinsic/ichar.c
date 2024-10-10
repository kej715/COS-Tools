long _ichar(unsigned long ref) {
    char *s;

    s = (char *)(ref & 0xffffffff);
    return (long)*s;
}
