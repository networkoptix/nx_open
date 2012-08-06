#ifndef __TRANSCODER_H
#define __TRANSCODER_H

#include <QString>
#include <QLocale>
#include "libavcodec/avcodec.h"
#include "core/datapacket/mediadatapacket.h"

/*
* Transcode input MediaPackets to specified format
*/
class QnTranscoder: public QObject
{
public:
    typedef QMap<QString, QVariant> Params;

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
    virtual bool setVideoCodec(CodecID codec, bool directStreamCopy = false, int width = 1920, int height = 1080, int bitrate = -1, const Params& params = Params()) = 0;


    /*
    * Set ffmpeg audio codec and params
    * @return Returns 0 if no error or error code
    * @param codec codec to transcode
    * @param directStreamCopy if true - do not transcode audio data, only put it to destination container
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected
    * @param addition codec params. Not used if directStreamCopy = true
    */
    virtual bool setAudioCodec(CodecID codec, bool directStreamCopy = false, int bitrate = -1, const Params& params = Params());

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
    bool m_videoStreamCopy;
    bool m_audioStreamCopy;
    bool m_isVideoPresent;
    bool m_isAudioPresent;
private:
    QString m_lastErrMessage;
    QQueue<QnCompressedVideoDataPtr> m_delayedVideoQueue;
    QQueue<QnCompressedAudioDataPtr> m_delayedAudioQueue;
    bool m_initialized;
};

#endif  // __TRANSCODER_H
