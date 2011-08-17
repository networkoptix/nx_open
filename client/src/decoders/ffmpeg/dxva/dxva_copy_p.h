#ifndef DXVA_COPY_P_H
#define DXVA_COPY_P_H

#include "dxva_picture_p.h"

struct copy_cache_t
{
    copy_cache_t()
        : base(0),
          buffer(0),
          size(0)
    {}

    void *base;
    unsigned char *buffer;
    size_t size;
};

bool CopyInitCache(copy_cache_t *cache, unsigned width);
void CopyCleanCache(copy_cache_t *cache);

void CopyFromNv12(picture_t *dst, unsigned char *src[2], size_t src_pitch[2],
                  unsigned width, unsigned height,
                  copy_cache_t *cache);
void CopyFromYv12(picture_t *dst, unsigned char *src[3], size_t src_pitch[3],
                  unsigned width, unsigned height,
                  copy_cache_t *cache);

#endif // DXVA_COPY_P_H
