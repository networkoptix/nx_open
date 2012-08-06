#ifndef __TRANSCODER_H
#define __TRANSCODER_H

#include <QString>
#include <QSharedPointer>
#include "libavcodec/avcodec.h"
#include "core/datapacket/mediadatapacket.h"

class QnCodecTranscoder
{
public:
    typedef QMap<QString, QVariant> Params;

    /*
    * Function provide addition information about transcoded context.
    * Function may be not implemented in derived classes and return 0
    * In this case, all necessary information MUST be present in bitstream. For example, SPS/PPS blocks for H.264
    */
    virtual AVCodecContext* getCodecContext();

    void setParams(const Params& params);
    void setBitrate(int value);

    /*
    * Transcode media packet and put data to 'result' variable
    * @return Return error code or 0 if no error
    */
    virtual int transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result) = 0;
private:
    Params m_params;
    int m_bitrate;
};
typedef QSharedPointer<QnCodecTranscoder> QnCodecTranscoderPtr;

class QnVideoTranscoder: public QnCodecTranscoder
{
public:
    void setSize(const QSize& size);
    QSize getSize() const;
private:
    QSize m_size;
};
typedef QSharedPointer<QnVideoTranscoder> QnVideoTranscoderPtr;

class QnAudioTranscoder: public QnCodecTranscoder
{
public:
};
typedef QSharedPointer<QnAudioTranscoder> QnAudioTranscoderPtr;

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
protected:
    QnVideoTranscoderPtr m_vTranscoder;
    QnAudioTranscoderPtr m_aTranscoder;
    CodecID m_videoCodec;
    CodecID m_audioCodec;
private:
    QString m_lastErrMessage;
    QQueue<QnCompressedVideoDataPtr> m_delayedVideoQueue;
    QQueue<QnCompressedAudioDataPtr> m_delayedAudioQueue;
    bool m_initialized;
};

#endif  // __TRANSCODER_H
