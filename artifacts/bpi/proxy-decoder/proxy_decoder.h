#pragma once

#include <cstdint>

#if !defined(PROXY_DECODER_API)
    #define PROXY_DECODER_API
#endif // !PROXY_DECODER_API

/**
 * Interface to an external lib 'libproxydecoder', which performs actual decoding of video frames.
 *
 * For each decodeTo...() method:
 * @param compressedFrame If null, the frame is considered absent.
 * @param outPtsUs Receives Pts of the produced frame (microseconds), which may be different to the
 * compressed frame due to buffering.
 * @return Negative value on error, 0 on buffering (when there is no output frame), positive
 * value (frame number) on producing a frame.
 *
 * For each create...() static method:
 * If initialization fails, fail with assert().
 */
class PROXY_DECODER_API ProxyDecoder // interface
{
public:
    static ProxyDecoder* create(int frameWidth, int frameHeight);
    static ProxyDecoder* createImpl(int frameWidth, int frameHeight);
    static ProxyDecoder* createStub(int frameWidth, int frameHeight);

    virtual ~ProxyDecoder() = default;

    /**
     * Input data for the decoder, a single frame.
     */
    struct CompressedFrame
    {
        static const int kPaddingSize = 32; ///< Number of bytes allocated in data after dataSize.

        const uint8_t* data;
        int dataSize;
        bool isKeyFrame;
        int64_t ptsUs;
    };

    /**
     * Decode a frame to an already allocated ARGB buffer.
     */
    virtual int decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t* argbBuffer, int argbLineSize) = 0;

    /**
     * Decode a frame to a three already allocated Y, U and V buffers, where U and V buffers have
     * one byte for each 2x2 pixel block.
     */
    virtual int decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize) = 0;

    /**
     * Decode a frame and return the address of existing native YUV buffer of video decoder.
     */
    virtual int decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t** outBuffer, int* outBufferSize) = 0;

    /**
     * Decode a frame for future displaying it with displayDecoded().
     * @param outFrameHandle Opaque handle to the decoded frame, or null if still buffering.
     */
    virtual int decodeToDisplayQueue(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        void **outFrameHandle) = 0;

    /**
     * Display previously decoded frame into the platform-specific framebuffer, fitting the frame
     * (preserving aspect ratio) into the specified rect or into full-screen if rect params are 0.
     * @param frameHandle Can be null, then do nothing.
     */
    virtual void displayDecoded(void* frameHandle, int x, int y, int width, int height) = 0;
};
