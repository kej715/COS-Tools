double _rdim(unsigned long waddra, unsigned long waddrb) {
    double a;
    double b;

    a = *((double *)(waddra << 3));
    b = *((double *)(waddrb << 3));
    return (a > b) ? a - b : 0.0;
}
