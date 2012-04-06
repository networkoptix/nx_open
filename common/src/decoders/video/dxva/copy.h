#ifndef _VLC_AVCODEC_COPY_H
#define _VLC_AVCODEC_COPY_H 1

#include "picture.h"

struct copy_cache_t
{
    copy_cache_t()
        : base(0),
          buffer(0),
          size(0)
    {
    }

    void    *base;
    uint8_t *buffer;
    size_t  size;
};

bool  CopyInitCache(copy_cache_t *cache, unsigned width);
void CopyCleanCache(copy_cache_t *cache);

void CopyFromNv12(picture_t *dst, uint8_t *src[2], size_t src_pitch[2],
                  unsigned width, unsigned height,
                  copy_cache_t *cache);
void CopyFromYv12(picture_t *dst, uint8_t *src[3], size_t src_pitch[3],
                  unsigned width, unsigned height,
                  copy_cache_t *cache);

#endif

