int compare_u16(const void *a, const void *b) {
    uint16_t ua = *(const uint16_t *)a;
    uint16_t ub = *(const uint16_t *)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

