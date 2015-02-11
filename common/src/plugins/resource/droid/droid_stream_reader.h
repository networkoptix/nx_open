#ifndef droid_stream_reader_h_1756
#define droid_stream_reader_h_1756

#ifdef ENABLE_DROID

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"
#include "droid_resource.h"
#include "utils/network/socket.h"
#include "utils/network/rtpsession.h"
#include "utils/network/h264_rtp_parser.h"

class PlDroidStreamReader: public CLServerPushStreamReader
{
public:
    static void setSDPInfo(quint32 ipv4, QByteArray sdpInfo);

    PlDroidStreamReader(const QnResourcePtr& res);
    virtual ~PlDroidStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

private:
    static QnMutex m_allReadersMutex;
    static QMap<quint32, PlDroidStreamReader*> m_allReaders;
    void setSDPInfo(QByteArray sdpInfo);
private:
    QnMutex m_controlPortSync;
    std::unique_ptr<AbstractStreamSocket> m_tcpSock;
    //UDPSocket* m_videoSock;
    //UDPSocket* m_audioSock;

    QnDroidResourcePtr m_droidRes;

    int m_connectionPort;
    int m_videoPort;
    int m_audioPort;
    int m_dataPort;
    RTPSession m_rtpSession;
    RTPIODevice* m_videoIoDevice;
    RTPIODevice* m_audioIoDevice;
    CLH264RtpParser* m_h264Parser;
    bool m_gotSDP;
};

#endif // #ifdef ENABLE_DROID
#endif //dlink_stream_reader_h_0251
