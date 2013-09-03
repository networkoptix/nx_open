#ifndef ACTI_STREAM_REDER_H__
#define ACTI_STREAM_REDER_H__

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/ffmpeg_rtp_parser.h"

class QnDesktopCameraStreamReader: public CLServerPushStreamReader
{
public:
    QnDesktopCameraStreamReader(QnResourcePtr res);
    virtual ~QnDesktopCameraStreamReader();
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

    virtual void pleaseStop() override;
private:
    TCPSocketPtr m_socket;
    quint8 m_recvBuffer[65536];
    QnFfmpegRtpParser parser;
    QTime m_keepaliveTimer;
};

#endif // ACTI_STREAM_REDER_H__
