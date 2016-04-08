#pragma once

#include <cstdint>
#include <memory>

class ProxyDecoderPrivate;

/**
 * Interface to an external lib 'proxydecoder', which performs actual decoding of a video frame.
 */
class ProxyDecoder
{
public:
    /**
     * If initialization fails, fail with 'assert'.
     */
    ProxyDecoder(int frameWidth, int frameHeight);

    virtual ~ProxyDecoder();

    /**
     * Input data for the decoder, a single frame.
     */
    struct CompressedFrame
    {
        static const int kPaddingSize = 32; ///< Number of bytes allocated in data after dataSize.

        const uint8_t* data;
        int dataSize;
        bool isKeyFrame;
        int64_t pts;
    };

    /**
     * Decode a frame to an already allocated ARGB buffer.
     * @param compressedFrame If null, the frame is considered absent.
     * @param outPts Receives Pts of the produced frame, which may be different to the compressed
     * frame due to buffering.
     * @return Negative value on error, 0 on buffering (when there is no output frame), positive
     * value (frame number) on producing a frame.
     */
    int decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t* argbBuffer, int argbLineSize);

    /**
     * Decode a frame to a three already allocated Y, U and V buffers, where U and V buffers have
     * one byte for each 2x2 pixel block.
     * @param compressedFrame If null, the frame is considered absent.
     * @param outPts Receives Pts of the produced frame, which may be different to the compressed
     * frame due to buffering.
     * @return Negative value on error, 0 on buffering (when there is no output frame), positive
     * value (frame number) on producing a frame.
     */
    int decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize);

    /**
     * Decode a frame and return the address of existing native YUV buffer of video decoder.
     * @param compressedFrame If null, the frame is considered absent.
     * @param outPts Receives Pts of the produced frame, which may be different to the compressed
     * frame due to buffering.
     * @return Negative value on error, 0 on buffering (when there is no output frame), positive
     * value (frame number) on producing a frame.
     */
    int decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t** outBuffer, int* outBufferSize);

private:
    std::unique_ptr<ProxyDecoderPrivate> d;
};
