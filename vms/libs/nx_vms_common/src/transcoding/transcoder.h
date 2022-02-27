// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QQueue>
#include <QSharedPointer>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include "nx/streaming/audio_data_packet.h"
#include "nx/streaming/video_data_packet.h"
#include "filters/abstract_image_filter.h"

#include <common/common_globals.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
#include "decoders/video/ffmpeg_video_decoder.h"
#include <nx/utils/move_only_func.h>

class CLVideoDecoderOutput;

namespace nx { namespace metrics { struct Storage; } }

/*!
    \note All constants (except \a quality) in this namespace refer to libavcodec CodecContex field names
*/
namespace QnCodecParams
{
    typedef QMap<QByteArray, QVariant> Value;

    static const QByteArray quality( "quality" );

    static const QByteArray qmin( "qmin" );
    static const QByteArray qmax( "qmax" );
    static const QByteArray qscale( "qscale" );
    static const QByteArray global_quality( "global_quality" );
}

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
        To empty transcoder'a coded picture buffer one should provide NULL as input until receiving NULL at output.
        \param media Coded picture of the input stream. May be NULL
        \param result Coded picture of the output stream. If NULL, only decoding is done
        \return return Return error code or 0 if no error
    */
    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) = 0;
    QString getLastError() const;
    virtual void setQuality( Qn::StreamQuality quality );
    virtual bool existMoreData() const { return false; }
    static QRect roundRect(const QRect& srcRect);
    static QSize roundSize(const QSize& size);

protected:
    QString m_lastErrMessage;
    QnCodecParams::Value m_params;
    int m_bitrate;
    AVCodecID m_codecId;
    Qn::StreamQuality m_quality;
};
typedef QSharedPointer<QnCodecTranscoder> QnCodecTranscoderPtr;

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
};
typedef QSharedPointer<QnAudioTranscoder> QnAudioTranscoderPtr;

class QnFfmpegVideoTranscoder;

//!Multiplexes one or more raw media streams into container format. Can apply transcoding to input media streams
/*
* Transcode input MediaPackets to specified format
*/
class NX_VMS_COMMON_API QnTranscoder: public QObject
{
    Q_OBJECT

public:
    QnTranscoder(const DecoderConfig& decoderConfig, nx::metrics::Storage* metrics);
    virtual ~QnTranscoder();

    enum TranscodeMethod {TM_DirectStreamCopy, TM_FfmpegTranscode, TM_QuickSyncTranscode, TM_OpenCLTranscode, TM_Dummy};

    enum OperationResult {
        Success = 0,
        Error = -1
    };

    /*
    * Set ffmpeg container by name
    * @return Returns 0 if no error or error code
    */
    virtual int setContainer(const QString& value) = 0;

    /*
    * Set ffmpeg video codec and params
    * @return Returns OperationResult::Success if no error or error code otherwise
    * @param codec codec to transcode
    * @param method how to transcode: no transcode, software, GPU e.t.c
    * @resolution output resolution. If not valid, than source resolution will be used. Not used if transcode method TM_NoTranscode
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected. Not used if transcode method TM_NoTranscode
    * @param addition codec params. Not used if transcode method TM_NoTranscode
    */
    virtual int setVideoCodec(
        AVCodecID codec,
        TranscodeMethod method,
        Qn::StreamQuality quality,
        const QSize& resolution = QSize(),
        int bitrate = -1,
        QnCodecParams::Value params = QnCodecParams::Value());

    /*
    * Set ffmpeg audio codec and params
    * @return Returns OperationResult::Success if no error or error code otherwise
    * @param codec codec to transcode
    * @param directStreamCopy if true - do not transcode audio data, only put it to destination container
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected
    * @param addition codec params. Not used if directStreamCopy = true
    */
    virtual OperationResult setAudioCodec(AVCodecID codec, TranscodeMethod method);

    /*
    * Transcode media data and write it to specified QnByteArray
    * @param result transcoded data block. If NULL, only decoding is done
    * @return Returns OperationResult::Success if no error or error code otherwise
    */
    int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnByteArray* const result);
    //!Flushes codec buffer and finalizes stream. No \a QnTranscoder::transcodePacket is allowed after this call
    /*
    * @return Returns OperationResult::Success if no error or error code otherwise
    */
    int finalize(QnByteArray* const result);
    //!Adds tag to the file. Maximum langth of tags and allowed names are format dependent
    /*!
        This implementation always returns \a false
        \return true on success
    */
    virtual bool addTag( const QString& name, const QString& value );

    /**
     * Return description of the last error code
     */
    QString getLastErrorMessage() const;

    // for internal use only. move to protectd!
    int writeBuffer(const char* data, int size);
    void setPacketizedMode(bool value);
    const QVector<int>& getPacketsSize();

    /**
     * Selects media stream parameters based on codec, resolution and quality
     */
    static QnCodecParams::Value suggestMediaStreamParams(
        AVCodecID codec,
        Qn::StreamQuality quality);

    /**
     * Suggest media bitrate based on codec, resolution and quality
     */
    static int suggestBitrate(
        AVCodecID codec,
        QSize resolution,
        Qn::StreamQuality quality,
        const char* codecName = nullptr);

    void setTranscodingSettings(const QnLegacyTranscodingSettings& settings);

    void setUseRealTimeOptimization(bool value);

    using BeforeOpenCallback = nx::utils::MoveOnlyFunc<void(
        QnTranscoder* transcoder,
        const QnConstCompressedVideoDataPtr & video,
        const QnConstCompressedAudioDataPtr & audio)>;

    void setBeforeOpenCallback(BeforeOpenCallback callback);
protected:
    /*
    *  Prepare to transcode. If 'direct stream copy' is used, function got not empty video and audio data
    * Destionation codecs MUST be used from source data codecs. If 'direct stream copy' is false, video or audio may be empty
    * @return Returns OperationResult::Success if no error or error code otherwise
    */
    virtual int open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio) = 0;

    virtual int transcodePacketInternal(const QnConstAbstractMediaDataPtr& media, QnByteArray* const result) = 0;
    virtual int finalizeInternal(QnByteArray* const result) = 0;
private:
    int openAndTranscodeDelayedData(QnByteArray* const result);

protected:

    DecoderConfig m_decoderConfig;
    QSharedPointer<QnFfmpegVideoTranscoder> m_vTranscoder;
    QnAudioTranscoderPtr m_aTranscoder;
    AVCodecID m_videoCodec;
    AVCodecID m_audioCodec;
    bool m_videoStreamCopy;
    bool m_audioStreamCopy;
    QnByteArray m_internalBuffer;
    QVector<int> m_outputPacketSize;
    qint64 m_firstTime;
protected:
    bool m_initialized;
    //! Make sure to correctly fill these member variables in overriden open() function.
    bool m_initializedAudio;    // Incoming audio packets will be ignored.
    bool m_initializedVideo;    // Incoming video packets will be ignored.
    nx::metrics::Storage* m_metrics = nullptr;
private:
    QString m_lastErrMessage;
    QQueue<QnConstCompressedVideoDataPtr> m_delayedVideoQueue;
    QQueue<QnConstCompressedAudioDataPtr> m_delayedAudioQueue;
    int m_eofCounter;
    bool m_packetizedMode;
    QnLegacyTranscodingSettings m_transcodingSettings;
    bool m_useRealTimeOptimization;
    BeforeOpenCallback m_beforeOpenCallback;
};

typedef QSharedPointer<QnTranscoder> QnTranscoderPtr;
