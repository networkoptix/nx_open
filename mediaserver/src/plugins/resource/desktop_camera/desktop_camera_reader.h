#ifndef DESKTOP_CAMERA_STREAM_REDER_H__
#define DESKTOP_CAMERA_STREAM_REDER_H__

#ifdef ENABLE_DESKTOP_CAMERA

#include <QtCore/QElapsedTimer>

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/ffmpeg_rtp_parser.h"

class QnDesktopCameraStreamReader: public CLServerPushStreamReader
{
public:
    QnDesktopCameraStreamReader(const QnResourcePtr& res);
    virtual ~QnDesktopCameraStreamReader();
    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

    virtual void pleaseStop() override;
private:
    int processTextResponse();
private:
    TCPSocketPtr m_socket;
    quint8 m_recvBuffer[65536];
    QnFfmpegRtpParser m_parsers[2];
    QElapsedTimer m_keepaliveTimer;
    QnResourceCustomAudioLayoutPtr m_audioLayout;
    mutable QMutex m_audioLayoutMutex;
};

#endif  //ENABLE_DESKTOP_CAMERA

#endif // DESKTOP_CAMERA_STREAM_REDER_H__
