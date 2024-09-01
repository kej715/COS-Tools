static char buf[16];
static int  next = 0;

unsigned long _char(long *x) {
    unsigned long ref;

    buf[next] = (char)*x;
    ref = (1 << 32) | (unsigned long)(&buf[next]);
    next = (next + 1) & 0x0f;
    return ref;
}