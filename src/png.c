#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void put_u32(uint8_t b[4], uint32_t v)
{
    b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);  b[3] = (uint8_t)v;
}

static void write_chunk(FILE *f, const char type[4], const uint8_t *data, uint32_t len)
{
    uint8_t h[8];
    put_u32(h, len);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if(len) {
        crc = crc32(crc, (const Bytef *)data, len);
        fwrite(data, 1, len, f);
    }
    uint8_t c[4];
    put_u32(c, crc);
    fwrite(c, 1, 4, f);
}

int png_save(const char *path, const uint8_t *argb, int w, int h, int stride)
{
    if(!path || !argb || w <= 0 || h <= 0) return -1;
    size_t rowbytes = (size_t)w * 4;
    size_t raw_size = (rowbytes + 1) * (size_t)h;
    uint8_t *raw = (uint8_t *)malloc(raw_size);
    if(!raw) return -1;
    for(int y = 0; y < h; y++) {
        uint8_t *dst = raw + (size_t)y * (rowbytes + 1);
        dst[0] = 0; /* filter: None */
        const uint8_t *src = argb + (size_t)y * (size_t)stride;
        for(int x = 0; x < w; x++) {
            dst[1 + x * 4 + 0] = src[x * 4 + 2]; /* R */
            dst[1 + x * 4 + 1] = src[x * 4 + 1]; /* G */
            dst[1 + x * 4 + 2] = src[x * 4 + 0]; /* B */
            dst[1 + x * 4 + 3] = 255;            /* A opaque */
        }
    }
    uLongf comp_len = compressBound((uLong)raw_size);
    uint8_t *comp = (uint8_t *)malloc(comp_len);
    if(!comp) { free(raw); return -1; }
    if(compress2(comp, &comp_len, raw, (uLong)raw_size, 9) != Z_OK) {
        free(raw); free(comp); return -1;
    }
    free(raw);

    FILE *f = fopen(path, "wb");
    if(!f) { free(comp); return -1; }
    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    put_u32(ihdr, (uint32_t)w);
    put_u32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    write_chunk(f, "IHDR", ihdr, 13);
    write_chunk(f, "IDAT", comp, (uint32_t)comp_len);
    write_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(comp);
    return 0;
}