#ifndef __TEST_CAMERA_STREAM_READER_H__
#define __TEST_CAMERA_STREAM_READER_H__

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"
#include "core/dataprovider/live_stream_provider.h"
#include "testcamera_resource.h"
#include "utils/network/socket.h"
#include "utils/network/rtpsession.h"
#include "utils/network/h264_rtp_parser.h"

class QnTestCameraStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    QnTestCameraStreamReader(QnResourcePtr res);
    virtual ~QnTestCameraStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
    int receiveData(quint8* buffer, int size);
private:
    TCPSocket m_tcpSock;
    QnMediaContextPtr m_context;
};

#endif // __TEST_CAMERA_STREAM_READER_H__
