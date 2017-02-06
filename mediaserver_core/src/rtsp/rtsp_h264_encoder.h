#ifndef __RTSP_H264_ENCODER_H__
#define __RTSP_H264_ENCODER_H__

#include <QtCore/QMap>
#include "rtsp/rtsp_encoder.h"

static const int RTSP_H264_PAYLOAD_TYPE = 96;
static const int RTSP_H264_PAYLOAD_FREQ = 90000;
static const int RTSP_H264_PAYLOAD_SSRC = 15000;
static const int RTSP_H264_MAX_LEN = 1412;

class QnRtspH264Encoder: public QnRtspEncoder
{
public:
    QnRtspH264Encoder();

    virtual QByteArray getAdditionSDP( const std::map<QString, QString>& streamParams ) override;

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(QnByteArray& sendBuffer) override;
    virtual void init() override;

    virtual quint32 getSSRC() override;
    virtual bool getRtpMarker() override;
    virtual quint32 getFrequency() override;
    virtual quint8 getPayloadtype() override;
    virtual QString getName() override;

    virtual bool isRtpHeaderExists() const override { return false; }
private:
    void goNextNal();
    bool isLastPacket();
private:
    QnConstAbstractMediaDataPtr m_media;
    const quint8* m_currentData;
    const quint8* m_nalEnd;
    bool m_nalStarted;
    bool m_isFragmented;
    quint8 m_nalType;
    quint8 m_nalRefIDC;
};

#endif // __RTSP_H264_ENCODER_H__
