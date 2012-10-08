#ifndef __RTSP_FFMPEG_ENCODER_H__
#define __RTSP_FFMPEG_ENCODER_H__

#include <QMap>
#include "rtsp_encoder.h"

class QnRtspFfmpegEncoder: public QnRtspEncoder
{
public:
    QnRtspFfmpegEncoder();

    virtual QByteArray getAdditionSDP() override;

    virtual void setDataPacket(QnAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(QnByteArray& sendBuffer) override;
    virtual void init() override;

    virtual quint32 getSSRC() override;
    virtual bool getRtpMarker() override;
    virtual quint32 getFrequency() override;
    virtual quint8 getPayloadtype() override;
    virtual QString getName() override;

    void setLiveMarker(int value);
    void setAdditionFlags(quint16 value);

    virtual bool isRtpHeaderExists() const override { return false; }

    void setCodecContext(QnMediaContextPtr context);
private:
    QnMediaContextPtr getGeneratedContext(CodecID compressionType);
private:
    bool m_gotLivePacket;
    QMap<int, QList<QnMediaContextPtr> > m_ctxSended;
    QMap<CodecID, QnMediaContextPtr> m_generatedContext;
    QnAbstractMediaDataPtr m_media;
    const char* m_curDataBuffer;
    QByteArray m_codecCtxData;
    int m_liveMarker;
    quint16 m_additionFlags;
    bool m_eofReached;
    bool m_isLastDataContext;
};

typedef QSharedPointer<QnRtspFfmpegEncoder> QnRtspFfmpegEncoderPtr;

#endif // __RTSP_FFMPEG_ENCODER_H__
