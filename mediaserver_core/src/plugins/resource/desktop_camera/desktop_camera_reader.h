#ifndef DESKTOP_CAMERA_STREAM_REDER_H__
#define DESKTOP_CAMERA_STREAM_REDER_H__

#ifdef ENABLE_DESKTOP_CAMERA

#include <QtCore/QElapsedTimer>

#include <providers/spush_media_stream_provider.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/nx_rtp_parser.h>
#include <network/multicodec_rtp_reader.h>
#include <nx/network/deprecated/simple_http_client.h>

class QnDesktopCameraStreamReader: public CLServerPushStreamReader
{
public:
    QnDesktopCameraStreamReader(const QnResourcePtr& res);
    virtual ~QnDesktopCameraStreamReader();
    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;
    void setNeedVideoData(bool value);
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;

    /*
     * isCameraControlRequired param using for DesktopCamera as:
     * - value 'false' means open audio stream only
     * - value 'true' means audio + video
     */
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual void pleaseReopenStream() override;
    virtual void pleaseStop() override;

    /**
    * A bit hack here: if open desktop camera without configuration then only audio stream is opened.
    * Otherwise (default) video + audio is opened
    */
    virtual bool isCameraControlRequired() const override;
private:
    int processTextResponse();
    QnAbstractMediaDataPtr createEmptyPacket();
private:
    static const int MEDIA_STREAM_COUNT = 2;

    TCPSocketPtr m_socket;
    quint8 m_recvBuffer[65536];
    QnNxRtpParser m_parsers[MEDIA_STREAM_COUNT];
    QElapsedTimer m_keepaliveTimer;
    QnResourceCustomAudioLayoutPtr m_audioLayout;
    mutable QnMutex m_audioLayoutMutex;
};

#endif  //ENABLE_DESKTOP_CAMERA

#endif // DESKTOP_CAMERA_STREAM_REDER_H__
