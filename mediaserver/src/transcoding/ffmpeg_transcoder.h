#ifndef __FFMPEG_TRANSCODER_H
#define __FFMPEG_TRANSCODER_H

#include "libavcodec/avcodec.h"
#include "transcoder.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"

const static int MAX_VIDEO_FRAME = 1024 * 3;

/*
* Transcode input MediaPackets to specified format
*/
class QnFfmpegTranscoder: public QnTranscoder
{
public:
    typedef QMap<QString, QVariant> Params;

    QnFfmpegTranscoder();
    ~QnFfmpegTranscoder();

    /*
    * Set ffmpeg container by name
    * @return Returns 0 if no error or error code
    */
    int setContainer(const QString& value);

    /*
    * Set ffmpeg video codec and params
    * @return Returns 0 if no error or error code
    * @param codec codec to transcode
    * @param directStreamCopy if true - do not transcode video data, only put it to destination container
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected
    * @param addition codec params. Not used if directStreamCopy = true
    */
    bool setVideoCodec(CodecID codec, bool directStreamCopy = false, int width = 1920, int height = 1080, int bitrate = -1, const Params& params = Params());


    /*
    * Set ffmpeg audio codec and params
    * @return Returns 0 if no error or error code
    * @param codec codec to transcode
    * @param directStreamCopy if true - do not transcode audio data, only put it to destination container
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected
    * @param addition codec params. Not used if directStreamCopy = true
    */
    bool setAudioCodec(CodecID codec, bool directStreamCopy = false, int bitrate = -1, const Params& params = Params());

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

    qint32 write(quint8* buf, int size);
protected:
    virtual int open(QnCompressedVideoDataPtr video, QnCompressedAudioDataPtr audio) override;
    virtual int transcodePacketInternal(QnAbstractMediaDataPtr media, QnByteArray& result) override;
private:
    friend qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size);
    AVIOContext* createFfmpegIOContext();
    void closeFfmpegContext();
private:
    CLFFmpegVideoDecoder* m_videoDecoder;
    CLVideoDecoderOutput m_decodedVideoFrame;
    AVCodecContext* m_videoEncoderCodecCtx;
    AVCodecContext* m_audioEncoderCodecCtx;
    quint8* m_videoEncodingBuffer;
    int m_videoBitrate;
    qint64 m_startDateTime;
    AVFormatContext* m_formatCtx;
    QString m_lastErrMessage;
   
    QnByteArray* m_dstByteArray;
    AVIOContext* m_ioContext;
    QString m_container;
};

#endif  // __FFMPEG_TRANSCODER_H
