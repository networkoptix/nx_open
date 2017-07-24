#pragma once

#include <cstdint>

#if !defined(DETECTION_PLUGIN_API) //< Linking attributes
    #define DETECTION_PLUGIN_API
#endif // !DETECTION_PLUGIN_API

/**
 * Interface to an external lib 'libdetection_plugin', which performs processing of video frames.
 * ATTENTION: This interface is intentionally kept pure C++ and does not depend on other libs.
 */
class DETECTION_PLUGIN_API DetectionPlugin
{
public:
    struct Params
    {
        const char* id = "undefined_id";
        const char* modelFile = "undefined_modelFile";
        const char* deployFile = "undefined_deployFile";
        const char* cacheFile = "undefined_cacheFile";
    };

    static DetectionPlugin* create(const Params& params);
    static DetectionPlugin* createImpl(const Params& params);
    static DetectionPlugin* createStub(const Params& params);

    virtual ~DetectionPlugin() = default;

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
