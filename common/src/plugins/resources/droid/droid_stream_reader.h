#ifndef droid_stream_reader_h_1756
#define droid_stream_reader_h_1756

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"
#include "core/dataprovider/live_stream_provider.h"
#include "droid_resource.h"
#include "utils/network/socket.h"
#include "utils/network/rtpsession.h"
#include "utils/network/h264_rtp_parser.h"

class PlDroidStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    PlDroidStreamReader(QnResourcePtr res);
    virtual ~PlDroidStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

private:


private:

    TCPSocket m_tcpSock;
    UDPSocket* m_videoSock;
    UDPSocket* m_audioSock;
    UDPSocket* m_dataSock;
    QnDroidResourcePtr m_droidRes;

    int m_connectionPort;
    int m_videoPort;
    int m_audioPort;
    int m_dataPort;
    RTPSession m_rtpSession;
    RTPIODevice m_ioDevice;
    CLH264RtpParser m_h264Parser;
};

#endif //dlink_stream_reader_h_0251