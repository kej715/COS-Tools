long _idim(unsigned long waddra, unsigned long waddrb) {
    long a;
    long b;

    a = *((long *)(waddra << 3));
    b = *((long *)(waddrb << 3));
    return (a > b) ? a - b : 0;
}
