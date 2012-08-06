#ifndef __TRANSCODER_H
#define __TRANSCODER_H

#include <QString>
#include <QSharedPointer>
#include "libavcodec/avcodec.h"
#include "core/datapacket/mediadatapacket.h"


//!Base class for all raw media stream transcoders
/*!
    Transcoder receives input stream coded picture at the input and provides output stream coded picture at output.
    It is safe to change output stream parameters during transcoding
*/
class QnCodecTranscoder
{
public:
    typedef QMap<QString, QVariant> Params;

    QnCodecTranscoder(CodecID codecId);
    virtual ~QnCodecTranscoder() {}
    
    /*
    * Function provide additional information about transcoded context.
    * Function may be not implemented in derived classes and return NULL
    * In this case, all necessary information MUST be present in bitstream. For example, SPS/PPS blocks for H.264
    */
    virtual AVCodecContext* getCodecContext();

    //!Set codec-specific params for output stream. For list of supported params please refer to derived class' doc
    virtual void setParams(const Params& params);
    //!Returns coded-specific params
    const Params& getParams() const;
    //!Set output stream bitrate (bps)
    virtual void setBitrate(int value);
    //!Get output bitrate bitrate (bps)
    int getBitrate() const;

    //!Transcode coded picture of input stream to output format
    /*!
        Transcoder is allowed to return NULL coded picture for non-NULL input and return coded output pictures with some delay from input.
        To empty transcoder'a coded picture buffer one should provide NULL as input until receiving NULL at output.
        \param media Coded picture of the input stream. May be NULL
        \param result Coded picture of the output stream. May be NULL
        \return return Return error code or 0 if no error
    */
    virtual int transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result) = 0;
    QString getLastError() const;

private:
    Params m_params;
    int m_bitrate;
    CodecID m_codecId;
};
typedef QSharedPointer<QnCodecTranscoder> QnCodecTranscoderPtr;

//!Base class for all video transcoders
class QnVideoTranscoder: public QnCodecTranscoder
{
public:
    QnVideoTranscoder(CodecID codecId);

    //!Set picture size (in pixels) of output video stream
    /*!
        By default, output stream has the same picture size as input
    */
    virtual void setSize( const QSize& size );
    //!Returns picture size (in pixels) of output video stream
    QSize getSize() const;

private:
    QSize m_size;
};
typedef QSharedPointer<QnVideoTranscoder> QnVideoTranscoderPtr;

//!Base class for all audio transcoders
class QnAudioTranscoder: public QnCodecTranscoder
{
public:
    QnAudioTranscoder(CodecID codecId);
};
typedef QSharedPointer<QnAudioTranscoder> QnAudioTranscoderPtr;

//!Multiplexes one or more raw media streams into container format. Can apply transcoding to input media streams
/*
* Transcode input MediaPackets to specified format
*/
class QnTranscoder: public QObject
{
public:
    QnTranscoder();
    virtual ~QnTranscoder();

    /*
    * Set ffmpeg container by name
    * @return Returns 0 if no error or error code
    */
    virtual int setContainer(const QString& value) = 0;

    /*
    * Set ffmpeg video codec and params
    * @return Returns 0 if no error or error code
    * @param codec codec to transcode
    * @param directStreamCopy if true - do not transcode video data, only put it to destination container
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected
    * @param addition codec params. Not used if directStreamCopy = true
    */
    virtual bool setVideoCodec(CodecID codec, QnVideoTranscoderPtr vTranscoder = QnVideoTranscoderPtr());


    /*
    * Set ffmpeg audio codec and params
    * @return Returns 0 if no error or error code
    * @param codec codec to transcode
    * @param directStreamCopy if true - do not transcode audio data, only put it to destination container
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected
    * @param addition codec params. Not used if directStreamCopy = true
    */
    virtual bool setAudioCodec(CodecID codec, QnAudioTranscoderPtr aTranscoder = QnAudioTranscoderPtr());

    /*
    * Transcode media data and write it to specified QnByteArray
    * @param transcoded data block
    * @return Returns 0 if no error or error code
    */
    int transcodePacket(QnAbstractMediaDataPtr media, QnByteArray& result);

    /*
    * Return description of the last error code
    */
    QString getLastErrorMessage() const;

protected:
    /*
    *  Prepare to transcode. If 'direct stream copy' is used, function got not empty video and audio data
    * Destionation codecs MUST be used from source data codecs. If 'direct stream copy' is false, video or audio may be empty
    * @return true if successfull opened
    */
    virtual int open(QnCompressedVideoDataPtr video, QnCompressedAudioDataPtr audio) = 0;

    virtual int transcodePacketInternal(QnAbstractMediaDataPtr media, QnByteArray& result) = 0;

    QnVideoTranscoderPtr m_vTranscoder;
    QnAudioTranscoderPtr m_aTranscoder;
    CodecID m_videoCodec;
    CodecID m_audioCodec;
    bool m_videoStreamCopy;
    bool m_audioStreamCopy;

private:
    QString m_lastErrMessage;
    QQueue<QnCompressedVideoDataPtr> m_delayedVideoQueue;
    QQueue<QnCompressedAudioDataPtr> m_delayedAudioQueue;
    bool m_initialized;
};

#endif  // __TRANSCODER_H
