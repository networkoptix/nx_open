// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/media_data_packet.h>
#include <nx/media/video_data_packet.h>
#include <utils/media/frame_info.h>

#include "decoder_types.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

class QGLContext;

struct DecoderConfig
{
    MultiThreadDecodePolicy mtDecodePolicy = MultiThreadDecodePolicy::autoDetect;
    bool forceGrayscaleDecoding = false; //< Force grayscale decoding if true. Don't change current value if false.
    bool forceRgbaFormat = false; //< Forces YUV -> RGB conversion on decoder thread.
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
    // for movies: full = IPB, fast == IP only, fastest = I only
    enum DecodeMode {
        DecodeMode_NotDefined,
        DecodeMode_Full,
        DecodeMode_Fast,
        DecodeMode_Fastest
    };

    virtual ~QnAbstractVideoDecoder() {}
    //!Returns video decoder type, GPU or CPU.
    virtual bool hardwareDecoder() const = 0;

    /**
      * Decode video frame.
      * Set hardwareAccelerationEnabled flag if hardware acceleration was used
      * \param outFrame Made a pointer to pointer to allow in future return internal object to increase performance
      * \return true If \a outFrame is filled, false if no output frame
      * \note No error information is returned!
      */
    virtual bool decode( const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFrame ) = 0;

    virtual void setLightCpuMode( DecodeMode val ) = 0;

    //!Use multi-threaded decoding (if supported by implementation)
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
    virtual bool resetDecoder(const QnConstCompressedVideoDataPtr& data) = 0;

    // return status of last decode call. Success - 0, Error - other value.
    virtual int getLastDecodeResult() const = 0;
};
