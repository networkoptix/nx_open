#pragma once

#include <cstdint>

#if !defined(TEGRA_VIDEO_API) //< Linking attributes.
    #define TEGRA_VIDEO_API
#endif

/**
 * Interface to an external lib "libtegra_video.so", which performs processing of video frames.
 * ATTENTION: This interface is intentionally kept pure C++ and does not depend on other libs.
 */
class TEGRA_VIDEO_API TegraVideo
{
public:
    static TegraVideo* create();
    static TegraVideo* createImpl();
    static TegraVideo* createStub();

    TegraVideo() = default;
    virtual ~TegraVideo() = default;

    struct Params
    {
        const char* id = "undefined_id";
        const char* modelFile = "undefined_modelFile";
        const char* deployFile = "undefined_deployFile";
        const char* cacheFile = "undefined_cacheFile";
        int netWidth = 0; /**< Net input width (pixels). If 0, try to parse from deployFile. */
        int netHeight = 0; /**< Net input height (pixels). If 0, try to parse from deployFile. */
    };

    virtual bool start(const Params& params) = 0;

    virtual bool stop() = 0;

    /** Input data for the decoder, a single frame. */
    struct CompressedFrame
    {
        const uint8_t* data = nullptr;
        int dataSize = 0;
        int64_t ptsUs = 0;
    };

    /** @param compressedFrame Is not referenced after this call returns; the data is copied. */
    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) = 0;

    struct Rect
    {
        float x = 0;
        float y = 0;
        float w = 0;
        float h = 0;
    };

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectCount, int* outRectCount, int64_t* outPtsUs) = 0;

    virtual bool hasMetadata() const = 0;
};
