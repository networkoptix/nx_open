#pragma once

#include "rtsp/rtsp_encoder.h"
#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "utils/common/byte_array.h"
#include <nx/streaming/rtp/rtcp.h>


class QnCommonModule;

class QnUniversalRtpEncoder: public QnRtspEncoder
{
public:
    /*
    * RTP muxer.
    * param media - source media data
    * param transcodeToCodec - if codec specified, all media packets are transcoded to specified codec.
    * param videoSize - transcoded video size
    */
    QnUniversalRtpEncoder(
        QnCommonModule* commonModule,
        QnConstAbstractMediaDataPtr media,
        AVCodecID transcodeToCodec,
        const QSize& videoSize,
        const QnLegacyTranscodingSettings& extraTranscodeParams);

    virtual QByteArray getAdditionSDP( const std::map<QString, QString>& streamParams ) override;

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(QnByteArray& sendBuffer) override;
    virtual void init() override;

    virtual quint32 getSSRC() override;
    virtual bool getRtpMarker() override;
    virtual quint32 getFrequency() override;
    virtual quint8 getPayloadtype() override;
    virtual QString getName() override;

    virtual bool isRtpHeaderExists() const override { return true; }
    bool isOpened() const;
    void setUseRealTimeOptimization(bool value);
    void enableAbsoluteRtcpTimestamps();
    void enableOnvifExtension() { m_addOnvifHeaderExtension = true; }


private:
    QnByteArray m_outputBuffer;
    bool m_absoluteRtcpTimestamps = false;
    bool m_addOnvifHeaderExtension = false;
    int m_outputPos;
    int packetIndex;
    QnFfmpegTranscoder m_transcoder;
    AVCodecID m_codec;
    bool m_isVideo;
    //quint32 m_firstTime;
    //bool m_isFirstPacket;
    bool m_isOpened;
    nx::streaming::rtp::RtcpSenderReporter m_rtcpReporter;
};
