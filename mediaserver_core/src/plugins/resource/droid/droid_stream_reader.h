#ifndef droid_stream_reader_h_1756
#define droid_stream_reader_h_1756

#ifdef ENABLE_DROID

#include <providers/spush_media_stream_provider.h>
#include <nx/network/deprecated/simple_http_client.h>
#include "droid_resource.h"
#include <nx/network/system_socket.h>
#include <nx/streaming/rtsp_client.h>

class CLH264RtpParser;

class PlDroidStreamReader: public CLServerPushStreamReader
{
public:
    static void setSDPInfo(quint32 ipv4, QByteArray sdpInfo);

    PlDroidStreamReader(const QnResourcePtr& res);
    virtual ~PlDroidStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseReopenStream() override;

private:
    static QnMutex m_allReadersMutex;
    static QMap<quint32, PlDroidStreamReader*> m_allReaders;
    void setSDPInfo(QByteArray sdpInfo);
private:
    QnMutex m_controlPortSync;
    std::unique_ptr<nx::network::TCPSocket> m_tcpSock;
    //UDPSocket* m_videoSock;
    //UDPSocket* m_audioSock;

    QnDroidResourcePtr m_droidRes;

    int m_connectionPort;
    int m_videoPort;
    int m_audioPort;
    int m_dataPort;
    QnRtspClient m_rtpSession;
    QnRtspIoDevice* m_videoIoDevice;
    QnRtspIoDevice* m_audioIoDevice;
    CLH264RtpParser* m_h264Parser;
    bool m_gotSDP;
};

#endif // #ifdef ENABLE_DROID
#endif //dlink_stream_reader_h_0251
