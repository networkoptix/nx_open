#ifndef ABSTRACT_VIDEO_DECODER_H
#define ABSTRACT_VIDEO_DECODER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <utils/media/frame_info.h>

extern "C"
{
#include <libavutil/pixfmt.h>
}

class QGLContext;

enum class MultiThreadDecodePolicy
{
    autoDetect, //< off for small resolution, on for large resolution
    disabled,
    enabled
};

struct DecoderConfig
{
    MultiThreadDecodePolicy mtDecodePolicy = MultiThreadDecodePolicy::autoDetect;
};

//!Abstract interface. Every video decoder MUST implement this interface.
/*!
    Implementation of this class does not have to be thread-safe.

    Methods getWidth() and getHeight() return actual picture size (after scaling).\n
    To find out, whether decoder supports scaling, one must check output picture size.
*/
class QnAbstractVideoDecoder
{
public:
    // TODO: #Elric #enum
    // for movies: full = IPB, fast == IP only, fastest = I only
    enum DecodeMode {
        DecodeMode_NotDefined,
        DecodeMode_Full,
        DecodeMode_Fast,
        DecodeMode_Fastest
    };

    virtual ~QnAbstractVideoDecoder() {}
    virtual AVPixelFormat GetPixelFormat() const { return AV_PIX_FMT_NONE; }
    //!Returns memory type to which decoder places decoded frames (system memory or opengl)
    virtual MemoryType targetMemoryType() const = 0;

    /**
      * Decode video frame.
      * Set hardwareAccelerationEnabled flag if hardware acceleration was used
      * \param outFrame Made a pointer to pointer to allow in future return internal object to increase performance
      * \return true If \a outFrame is filled, false if no output frame
      * \note No error information is returned!
      */
    virtual bool decode( const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFrame ) = 0;

    virtual void setLightCpuMode( DecodeMode val ) = 0;

    //!Use ulti-threaded decoding (if supported by implementation)
    virtual void setMultiThreadDecodePolicy(MultiThreadDecodePolicy mtDecodingPolicy) = 0;

    //!Returns output picture size in pixels (after scaling if it is present)
    virtual int getWidth() const  { return 0; }
    //!Returns output picture height in pixels (after scaling if it is present)
    virtual int getHeight() const { return 0; }
    virtual double getSampleAspectRatio() const { return 1; };
    //!Reset decoder state (e.g. to reposition source stream)
    /*!
        \param data First encoded frame of new stream. It is recommended that this frame be IDR and contain sequence header
    */
    virtual void resetDecoder( const QnConstCompressedVideoDataPtr& data ) = 0;

    // return status of last decode call. Success - 0, Error - other value.
    virtual int getLastDecodeResult() const = 0;

    // Flush decoder buffer. It called in case of seek operation.
    virtual void resetDecoder() = 0;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // ABSTRACT_VIDEO_DECODER_H
