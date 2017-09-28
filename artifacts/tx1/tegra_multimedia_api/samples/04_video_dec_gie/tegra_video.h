#pragma once

#include <cstdint>

#if !defined(TEGRA_VIDEO_API) //< Linking attributes
    #define TEGRA_VIDEO_API
#endif // !TEGRA_VIDEO_API

/**
 * Interface to an external lib 'libtegra_video', which performs processing of video frames.
 * ATTENTION: This interface is intentionally kept pure C++ and does not depend on other libs.
 */
class TEGRA_VIDEO_API TegraVideo
{
public:
    struct Params
    {
        const char* id = "undefined_id";
        const char* modelFile = "undefined_modelFile";
        const char* deployFile = "undefined_deployFile";
        const char* cacheFile = "undefined_cacheFile";
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
        float width = 0;
        float height = 0;
    };

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) = 0;

    virtual bool hasMetadata() const = 0;
};
