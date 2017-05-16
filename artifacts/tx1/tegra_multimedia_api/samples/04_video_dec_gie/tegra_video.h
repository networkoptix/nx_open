#pragma once

#include <cstdint>

#if !defined(TEGRA_VIDEO_API) //< Linking attributes
    #define TEGRA_VIDEO_API
#endif // !TEGRA_VIDEO_API

/**
 * Interface to an external lib 'libtegra_video', which performs processing of video frames.
 * ATTENTION: This interface is intentionally kept pure C++ and does not depend on other libs.
 */
class TEGRA_VIDEO_API TegraVideo // interface
{
public:
    struct Params
    {
        const char* modelFile;
        const char* deployFile;
        const char* cacheFile;
    };

    static TegraVideo* create(const Params& params);
    static TegraVideo* createImpl(const Params& params);
    static TegraVideo* createStub(const Params& params);

    virtual ~TegraVideo() = default;

    /**
     * Input data for the decoder, a single frame.
     */
    struct CompressedFrame
    {
        const uint8_t* data;
        int dataSize;
        int64_t ptsUs;
    };

    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) = 0;

    struct Rect
    {
        float x;
        float y;
        float width;
        float height;
    };

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) = 0;

    virtual bool hasMetadata() const = 0;
};
