#ifndef __RTP_FFMPEG_ENCODER_H__
#define __RTP_FFMPEG_ENCODER_H__

#include "rtsp_encoder.h"
#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "utils/common/byte_array.h"

class QnUniversalRtpEncoder: public QnRtspEncoder
{
public:
    /*
    * RTP muxer.
    * param media - source media data
    * param transcodeToCodec - if codec specified, all media packets are transcoded to specified codec.
    * param videoSize - transcoded video size
    */
    QnUniversalRtpEncoder(QnAbstractMediaDataPtr media, CodecID transcodeToCodec = CODEC_ID_NONE, const QSize& videoSize = QSize(640,480));

    virtual QByteArray getAdditionSDP() override;

    virtual void setDataPacket(QnAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(QnByteArray& sendBuffer) override;
    virtual void init() override;

    virtual quint32 getSSRC() override;
    virtual bool getRtpMarker() override;
    virtual quint32 getFrequency() override;
    virtual quint8 getPayloadtype() override;
    virtual QString getName() override;

    virtual bool isRtpHeaderExists() const override { return true; }
    virtual void setBaseTime(qint64 value) override;
    bool isOpened() const;
private:
    QnByteArray m_outputBuffer;
    int m_outputPos;
    int packetIndex;
    QnFfmpegTranscoder m_transcoder;
    CodecID m_codec;
    bool m_isVideo;
    quint32 m_firstTime;
    bool m_isFirstPacket;
    bool m_isOpened;
};


#endif // __RTP_FFMPEG_ENCODER_H__
