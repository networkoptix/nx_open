#ifndef __TEST_CAMERA_STREAM_READER_H__
#define __TEST_CAMERA_STREAM_READER_H__

#ifdef ENABLE_TEST_CAMERA

#include <core/dataprovider/spush_media_stream_provider.h>
#include <utils/network/simple_http_client.h>
#include "testcamera_resource.h"
#include <utils/network/socket.h>
#include <network/rtsp_session.h>

class QnTestCameraStreamReader: public CLServerPushStreamReader
{
public:
    QnTestCameraStreamReader(const QnResourcePtr& res);
    virtual ~QnTestCameraStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;

    int receiveData(quint8* buffer, int size);
private:
    std::unique_ptr<AbstractStreamSocket> m_tcpSock;
    QnConstMediaContextPtr m_context;
};

#endif // #ifdef ENABLE_TEST_CAMERA
#endif // __TEST_CAMERA_STREAM_READER_H__
