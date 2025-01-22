// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QQueue>
#include <QtCore/QString>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
#include <nx/media/audio_data_packet.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/move_only_func.h>
#include <transcoding/transcoding_utils.h>

#include "filters/abstract_image_filter.h"

class CLVideoDecoderOutput;

namespace nx::metric { struct Storage; }

//!Base class for all raw media stream transcoders
/*!
    Transcoder receives input stream coded picture at the input and provides output stream coded picture at output.
    It is safe to change output stream parameters during transcoding
*/
class NX_VMS_COMMON_API QnCodecTranscoder
{
public:
    QnCodecTranscoder(AVCodecID codecId);
    virtual ~QnCodecTranscoder() {}

    /*
    * Function provide additional information about transcoded context.
    * Function may be not implemented in derived classes and return NULL
    * In this case, all necessary information MUST be present in bitstream. For example, SPS/PPS blocks for H.264
    */
    //virtual AVCodecContext* getCodecContext();

    //!Set codec-specific params for output stream. For list of supported params please refer to derived class' doc
    virtual void setParams(const QnCodecParams::Value& params);
    //!Returns coded-specific params
    const QnCodecParams::Value& getParams() const;
    //!Set output stream bitrate (bps)
    virtual void setBitrate(int value);
    //!Get output bitrate bitrate (bps)
    int getBitrate() const;

    //!Transcode coded picture of input stream to output format
    /*!
        Transcoder is allowed to return NULL coded picture for non-NULL input and return coded output pictures with some delay from input.
        To empty transcoder's coded picture buffer one should provide NULL as input until receiving NULL at output.
        \param media Coded picture of the input stream. May be NULL
        \param result Coded picture of the output stream. If NULL, only decoding is done
        \return return Return error code or 0 if no error
    */
    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) = 0;
    virtual void setQuality( Qn::StreamQuality quality );
    static QRect roundRect(const QRect& srcRect);
    static QSize roundSize(const QSize& size);

protected:
    QnCodecParams::Value m_params;
    int m_bitrate;
    AVCodecID m_codecId;
    Qn::StreamQuality m_quality;
};
using QnCodecTranscoderPtr = std::unique_ptr<QnCodecTranscoder>;

//!Base class for all video transcoders
class QnVideoTranscoder: public QnCodecTranscoder
{
public:
    QnVideoTranscoder(AVCodecID codecId);
    virtual bool open(const QnConstCompressedVideoDataPtr& video) = 0;
};

//!Base class for all audio transcoders
class QnAudioTranscoder: public QnCodecTranscoder
{
public:
    QnAudioTranscoder(AVCodecID codecId): QnCodecTranscoder(codecId) {}
    virtual bool open(const QnConstCompressedAudioDataPtr& /*video*/)  = 0;
    virtual void setSampleRate(int value) = 0;
};
typedef std::unique_ptr<QnAudioTranscoder> QnAudioTranscoderPtr;
