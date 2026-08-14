#ifndef PNG_H
#define PNG_H
#include <stdint.h>
int png_save(const char *path, const uint8_t *argb, int w, int h, int stride_bytes);
#endif