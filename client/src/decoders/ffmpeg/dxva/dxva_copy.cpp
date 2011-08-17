#include "dxva_copy_p.h"

#include <assert.h>

unsigned cpu_flags = 0;

unsigned vlc_CPU(void)
{
    return cpu_flags;
}

void *vlc_memalign(void **base, size_t alignment, size_t size)
{
    assert(alignment >= sizeof(void*));
    for (size_t t = alignment; t > 1; t >>= 1)
        assert((t&1) == 0);
#if defined(HAVE_POSIX_MEMALIGN)
    if (posix_memalign(base, alignment, size)) {
        *base = NULL;
        return NULL;
    }
    return *base;
#elif defined(HAVE_MEMALIGN)
    return *base = memalign(alignment, size);
#else
    *base = malloc(size + alignment - 1);
    unsigned char *p = (unsigned char *)*base;
    if (!p)
        return NULL;
    return (void*)((uintptr_t)(p + alignment - 1) & ~(alignment - 1));
#endif
}

/* Copy 64 bytes from srcp to dsp loading data with the SSE>=2 instruction load and
* storing data with the SSE>=2 instruction store.
*/
#define COPY64(dstp, srcp, load, store) \
    asm volatile (                      \
    load "  0(%[src]), %%xmm1\n"    \
    load " 16(%[src]), %%xmm2\n"    \
    load " 32(%[src]), %%xmm3\n"    \
    load " 48(%[src]), %%xmm4\n"    \
    store " %%xmm1,    0(%[dst])\n" \
    store " %%xmm2,   16(%[dst])\n" \
    store " %%xmm3,   32(%[dst])\n" \
    store " %%xmm4,   48(%[dst])\n" \
    : : [dst]"r"(dstp), [src]"r"(srcp) : "memory")

/* Execute the instruction op only if SSE2 is supported. */
// #define CAN_COMPILE_SSE2
#ifdef CAN_COMPILE_SSE2
#   define ASM_SSE2(cpu, op) do {          \
    if (cpu & CPU_CAPABILITY_SSE2)  \
    asm volatile (op);    \
    } while (0)
#else
#   define ASM_SSE2(cpu, op)
#endif

/* Optimized copy from "Uncacheable Speculative Write Combining" memory
* as used by some video surface.
* XXX It is really efficient only when SSE4.1 is available.
*/
static void CopyFromUswc(unsigned char *dst, size_t dst_pitch,
                         const unsigned char *src, size_t src_pitch,
                         unsigned width, unsigned height,
                         unsigned cpu)
{
    assert(((intptr_t)dst & 0x0f) == 0 && (dst_pitch & 0x0f) == 0);

    ASM_SSE2(cpu, "mfence");
    for (unsigned y = 0; y < height; y++) {
        const unsigned unaligned = (intptr_t)src & 0x0f;
        unsigned x;

        for (x = 0; x < unaligned; x++)
            dst[x] = src[x];

// #define CAN_COMPILE_SSE2
// #  define CPU_CAPABILITY_SSE2    (1<<7)

#ifdef CAN_COMPILE_SSE4_1
        if (cpu & CPU_CAPABILITY_SSE4_1) {
            if (!unaligned) {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movntdqa", "movdqa");
            } else {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movntdqa", "movdqu");
            }
        } else
#endif
#ifdef CAN_COMPILE_SSE2
            if (cpu & CPU_CAPABILITY_SSE2) {
                if (!unaligned) {
                    for (; x+63 < width; x += 64)
                        COPY64(&dst[x], &src[x], "movdqa", "movdqa");
                } else {
                    for (; x+63 < width; x += 64)
                        COPY64(&dst[x], &src[x], "movdqa", "movdqu");
                }
            }
#endif

            for (; x < width; x++)
                dst[x] = src[x];

            src += src_pitch;
            dst += dst_pitch;
    }
}

static void Copy2d(unsigned char *dst, size_t dst_pitch,
                   const unsigned char *src, size_t src_pitch,
                   unsigned width, unsigned height,
                   unsigned cpu)
{
    assert(((intptr_t)src & 0x0f) == 0 && (src_pitch & 0x0f) == 0);

    ASM_SSE2(cpu, "mfence");

    for (unsigned y = 0; y < height; y++) {
        unsigned x = 0;
        bool unaligned = ((intptr_t)dst & 0x0f) != 0;

#ifdef CAN_COMPILE_SSE2
        if (cpu & CPU_CAPABILITY_SSE2) {
            if (!unaligned) {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movdqa", "movntdq");
            } else {
                for (; x+63 < width; x += 64)
                    COPY64(&dst[x], &src[x], "movdqa", "movdqu");
            }
        }
#endif

        for (; x < width; x++)
            dst[x] = src[x];

        src += src_pitch;
        dst += dst_pitch;
    }
}

static void SplitUV(unsigned char *dstu, size_t dstu_pitch,
                    unsigned char *dstv, size_t dstv_pitch,
                    const unsigned char *src, size_t src_pitch,
                    unsigned width, unsigned height, unsigned cpu)
{
    const unsigned char shuffle[] = { 0, 2, 4, 6, 8, 10, 12, 14,
        1, 3, 5, 7, 9, 11, 13, 15 };
    const unsigned char mask[] = { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
        0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 };

    assert(((intptr_t)src & 0x0f) == 0 && (src_pitch & 0x0f) == 0);

    ASM_SSE2(cpu, "mfence");

    for (unsigned y = 0; y < height; y++) {
        unsigned x = 0;

#define LOAD64 \
    "movdqa  0(%[src]), %%xmm0\n" \
    "movdqa 16(%[src]), %%xmm1\n" \
    "movdqa 32(%[src]), %%xmm2\n" \
    "movdqa 48(%[src]), %%xmm3\n"

#define STORE2X32 \
    "movq   %%xmm0,   0(%[dst1])\n" \
    "movq   %%xmm1,   8(%[dst1])\n" \
    "movhpd %%xmm0,   0(%[dst2])\n" \
    "movhpd %%xmm1,   8(%[dst2])\n" \
    "movq   %%xmm2,  16(%[dst1])\n" \
    "movq   %%xmm3,  24(%[dst1])\n" \
    "movhpd %%xmm2,  16(%[dst2])\n" \
    "movhpd %%xmm3,  24(%[dst2])\n"

#ifdef CAN_COMPILE_SSSE3
        if (cpu & CPU_CAPABILITY_SSSE3) {
            for (x = 0; x < (width & ~31); x += 32) {
                asm volatile (
                    "movdqu (%[shuffle]), %%xmm7\n"
                    LOAD64
                    "pshufb  %%xmm7, %%xmm0\n"
                    "pshufb  %%xmm7, %%xmm1\n"
                    "pshufb  %%xmm7, %%xmm2\n"
                    "pshufb  %%xmm7, %%xmm3\n"
                    STORE2X32
                    : : [dst1]"r"(&dstu[x]), [dst2]"r"(&dstv[x]), [src]"r"(&src[2*x]), [shuffle]"r"(shuffle) : "memory");
            }
        } else
#endif
#ifdef CAN_COMPILE_SSE2
            if (cpu & CPU_CAPABILITY_SSE2) {
                for (x = 0; x < (width & ~31); x += 32) {
                    asm volatile (
                        "movdqu (%[mask]), %%xmm7\n"
                        LOAD64
                        "movdqa   %%xmm0, %%xmm4\n"
                        "movdqa   %%xmm1, %%xmm5\n"
                        "movdqa   %%xmm2, %%xmm6\n"
                        "psrlw    $8,     %%xmm0\n"
                        "psrlw    $8,     %%xmm1\n"
                        "pand     %%xmm7, %%xmm4\n"
                        "pand     %%xmm7, %%xmm5\n"
                        "pand     %%xmm7, %%xmm6\n"
                        "packuswb %%xmm4, %%xmm0\n"
                        "packuswb %%xmm5, %%xmm1\n"
                        "pand     %%xmm3, %%xmm7\n"
                        "psrlw    $8,     %%xmm2\n"
                        "psrlw    $8,     %%xmm3\n"
                        "packuswb %%xmm6, %%xmm2\n"
                        "packuswb %%xmm7, %%xmm3\n"
                        STORE2X32
                        : : [dst2]"r"(&dstu[x]), [dst1]"r"(&dstv[x]), [src]"r"(&src[2*x]), [mask]"r"(mask) : "memory");
                }
            }
#endif
#undef STORE2X32
#undef LOAD64

            for (; x < width; x++) {
                dstu[x] = src[2*x+0];
                dstv[x] = src[2*x+1];
            }
            src  += src_pitch;
            dstu += dstu_pitch;
            dstv += dstv_pitch;
    }
}

static void CopyPlane(unsigned char *dst, size_t dst_pitch, const unsigned char *src, size_t src_pitch,
                      unsigned char *cache, size_t cache_size,
                      unsigned width, unsigned height,
                      unsigned cpu)
{
    const unsigned w16 = (width+15) & ~15;
    const unsigned hstep = cache_size / w16;
    assert(hstep > 0);

    for (unsigned y = 0; y < height; y += hstep) {
        const unsigned hblock = hstep < height - y ? hstep : height - y;

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w16,
            src, src_pitch,
            width, hblock, cpu);

        /* Copy from our cache to the destination */
        Copy2d(dst, dst_pitch,
            cache, w16,
            width, hblock, cpu);

        /* */
        src += src_pitch * hblock;
        dst += dst_pitch * hblock;
    }

    ASM_SSE2(cpu, "mfence");
}

static void SplitPlanes(unsigned char *dstu, size_t dstu_pitch,
                        unsigned char *dstv, size_t dstv_pitch,
                        const unsigned char *src, size_t src_pitch,
                        unsigned char *cache, size_t cache_size,
                        unsigned width, unsigned height,
                        unsigned cpu)
{
    const unsigned w2_16 = (2*width+15) & ~15;
    const unsigned hstep = cache_size / w2_16;
    assert(hstep > 0);

    for (unsigned y = 0; y < height; y += hstep) {
        const unsigned hblock = hstep < height - y ? hstep : height - y;

        /* Copy a bunch of line into our cache */
        CopyFromUswc(cache, w2_16,
            src, src_pitch,
            2*width, hblock, cpu);

        /* Copy from our cache to the destination */
        SplitUV(dstu, dstu_pitch,
            dstv, dstv_pitch,
            cache, w2_16,
            width, hblock, cpu);

        /* */
        src  += src_pitch  * hblock;
        dstu += dstu_pitch * hblock;
        dstv += dstv_pitch * hblock;
    }

    ASM_SSE2(cpu, "mfence");
}

bool CopyInitCache(copy_cache_t *cache, unsigned width)
{
    cache->size = ((width + 0x0f) & ~ 0x0f) > 4096 ? ((width + 0x0f) & ~ 0x0f) : 4096;
    cache->buffer = (unsigned char*)vlc_memalign(&cache->base, 16, cache->size);
    if (!cache->base)
        return false;
    return true;
}

void CopyCleanCache(copy_cache_t *cache)
{
    if (cache->base)
        free(cache->base);

    cache->base   = NULL;
    cache->buffer = NULL;
    cache->size   = 0;
}

void CopyFromNv12(picture_t *dst, unsigned char *src[2], size_t src_pitch[2],
                  unsigned width, unsigned height,
                  copy_cache_t *cache)
{
    const unsigned cpu = vlc_CPU();

    /* */
    CopyPlane(dst->p[0].p_pixels, dst->p[0].i_pitch,
        src[0], src_pitch[0],
        cache->buffer, cache->size,
        width, height, cpu);
    SplitPlanes(dst->p[2].p_pixels, dst->p[2].i_pitch,
        dst->p[1].p_pixels, dst->p[1].i_pitch,
        src[1], src_pitch[1],
        cache->buffer, cache->size,
        width/2, height/2, cpu);

    ASM_SSE2(cpu, "emms");
}

void CopyFromYv12(picture_t *dst, unsigned char *src[3], size_t src_pitch[3],
                  unsigned width, unsigned height,
                  copy_cache_t *cache)
{
    const unsigned cpu = vlc_CPU();

    /* */
    for (unsigned n = 0; n < 3; n++) {
        const unsigned d = n > 0 ? 2 : 1;
        CopyPlane(dst->p[n].p_pixels, dst->p[n].i_pitch,
            src[n], src_pitch[n],
            cache->buffer, cache->size,
            width/d, height/d, cpu);
    }
    ASM_SSE2(cpu, "emms");
}
