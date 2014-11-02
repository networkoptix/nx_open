#ifndef __TRANSCODER_H
#define __TRANSCODER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QString>
#include <QSharedPointer>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include "core/datapacket/audio_data_packet.h"
#include "core/datapacket/video_data_packet.h"
#include "core/resource/media_resource.h"

class QnAbstractImageFilter;
class CLVideoDecoderOutput;

/*!
    \note All constants (except \a quality) in this namespace refer to libavcodec CodecContex field names
*/
namespace QnCodecParams
{
    typedef QMap<QString, QVariant> Value;

    static const QLatin1String quality( "quality" );

    static const QLatin1String qmin( "qmin" );
    static const QLatin1String qmax( "qmax" );
    static const QLatin1String qscale( "qscale" );
    static const QLatin1String global_quality( "global_quality" );
}


//!Base class for all raw media stream transcoders
/*!
    Transcoder receives input stream coded picture at the input and provides output stream coded picture at output.
    It is safe to change output stream parameters during transcoding
*/
class QnCodecTranscoder
{
public:
    QnCodecTranscoder(CodecID codecId);
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
protected:
    QString m_lastErrMessage;
    QnCodecParams::Value m_params;
    int m_bitrate;
    CodecID m_codecId;
    Qn::StreamQuality m_quality;
};
typedef QSharedPointer<QnCodecTranscoder> QnCodecTranscoderPtr;


//!Base class for all video transcoders
class QnVideoTranscoder: public QnCodecTranscoder
{
public:
    QnVideoTranscoder(CodecID codecId);
    virtual ~QnVideoTranscoder();

    //!Set picture size (in pixels) of output video stream
    /*!
        By default, output stream has the same picture size as input
        \ param value If (0,0) than input stream picture size is used
        \ if width=0, width MUST be autodetected (keep source aspect ratio)
    */
    virtual void setResolution( const QSize& value );
    //!Returns picture size (in pixels) of output video stream
    QSize getResolution() const;

    virtual bool open(const QnConstCompressedVideoDataPtr& video);

    virtual void addFilter(QnAbstractImageFilter* filter);

protected:
    static const int WIDTH_ALIGN = 16;
    static const int HEIGHT_ALIGN = 2;
        
    QSharedPointer<CLVideoDecoderOutput> processFilterChain(const QSharedPointer<CLVideoDecoderOutput>& decodedFrame);

protected:
    QSize m_resolution;
    QList<QnAbstractImageFilter*> m_filters;
    bool m_opened;
};
typedef QSharedPointer<QnVideoTranscoder> QnVideoTranscoderPtr;


//!Base class for all audio transcoders
class QnAudioTranscoder: public QnCodecTranscoder
{
public:
    QnAudioTranscoder(CodecID codecId): QnCodecTranscoder(codecId) {}
    virtual bool open(const QnConstCompressedAudioDataPtr& video) { Q_UNUSED(video) return true; }
};
typedef QSharedPointer<QnAudioTranscoder> QnAudioTranscoderPtr;


//!Multiplexes one or more raw media streams into container format. Can apply transcoding to input media streams
/*
* Transcode input MediaPackets to specified format
*/
class QnTranscoder: public QObject
{
    Q_OBJECT

public:
    QnTranscoder();
    virtual ~QnTranscoder();

    enum TranscodeMethod {TM_DirectStreamCopy, TM_FfmpegTranscode, TM_QuickSyncTranscode, TM_OpenCLTranscode, TM_Dummy};

    /*
    * Set ffmpeg container by name
    * @return Returns 0 if no error or error code
    */
    virtual int setContainer(const QString& value) = 0;

    void setVideoLayout(QnConstResourceVideoLayoutPtr layout);
    void setRotation(int angle);
    void setCustomAR(qreal ar);

    /*
    * Set ffmpeg video codec and params
    * @return Returns 0 if no error or error code
    * @param codec codec to transcode
    * @param method how to transcode: no transcode, software, GPU e.t.c
    * @resolution output resolution. Not used if transcode method TM_NoTranscode
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected. Not used if transcode method TM_NoTranscode
    * @param addition codec params. Not used if transcode method TM_NoTranscode
    */
    virtual int setVideoCodec(
        CodecID codec,
        TranscodeMethod method,
        Qn::StreamQuality quality,
        const QSize& resolution = QSize(1024,768),
        int bitrate = -1,
        QnCodecParams::Value params = QnCodecParams::Value());


    /*
    * Set ffmpeg audio codec and params
    * @return Returns 0 if no error or error code
    * @param codec codec to transcode
    * @param directStreamCopy if true - do not transcode audio data, only put it to destination container
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected
    * @param addition codec params. Not used if directStreamCopy = true
    */
    virtual int setAudioCodec(CodecID codec, TranscodeMethod method);

    /*
    * Transcode media data and write it to specified QnByteArray
    * @param result transcoded data block. If NULL, only decoding is done
    * @return Returns 0 if no error or error code
    */
    int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnByteArray* const result);
    //!Adds tag to the file. Maximum langth of tags and allowed names are format dependent
    /*!
        This implementation always returns \a false
        \return true on success
    */
    virtual bool addTag( const QString& name, const QString& value );

    /*
    * Return description of the last error code
    */
    QString getLastErrorMessage() const;

    // for internal use only. move to protectd!
    int writeBuffer(const char* data, int size);
    void setPacketizedMode(bool value);
    const QVector<int>& getPacketsSize();

    //!Selects media stream parameters based on \a resolution and \a quality
    /*!
        Can add parameters to \a params
        \parm codec 
        \return bitrate in kbps
        \note Does not modify existing parameters in \a params
    */
    static int suggestMediaStreamParams(
        CodecID codec,
        QSize resolution,
        Qn::StreamQuality quality,
        QnCodecParams::Value* const params = NULL );

protected:
    /*
    *  Prepare to transcode. If 'direct stream copy' is used, function got not empty video and audio data
    * Destionation codecs MUST be used from source data codecs. If 'direct stream copy' is false, video or audio may be empty
    * @return true if successfull opened
    */
    virtual int open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio) = 0;

    virtual int transcodePacketInternal(const QnConstAbstractMediaDataPtr& media, QnByteArray* const result) = 0;

    QnVideoTranscoderPtr m_vTranscoder;
    QnAudioTranscoderPtr m_aTranscoder;
    CodecID m_videoCodec;
    CodecID m_audioCodec;
    bool m_videoStreamCopy;
    bool m_audioStreamCopy;
    QnByteArray m_internalBuffer;
    QVector<int> m_outputPacketSize;
    qint64 m_firstTime;

protected:
    bool m_initialized;
    QnConstResourceVideoLayoutPtr m_vLayout;
private:
    QString m_lastErrMessage;
    QQueue<QnConstCompressedVideoDataPtr> m_delayedVideoQueue;
    QQueue<QnConstCompressedAudioDataPtr> m_delayedAudioQueue;
    int m_eofCounter;
    bool m_packetizedMode;
    int m_rotAngle;
    qreal m_customAR;
};

typedef QSharedPointer<QnTranscoder> QnTranscoderPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif  // __TRANSCODER_H
