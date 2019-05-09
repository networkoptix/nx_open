#pragma once

#ifdef ENABLE_TEST_CAMERA

#include <providers/spush_media_stream_provider.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/network/socket.h>

#include "testcamera_resource.h"

class QnTestCameraStreamReader: public CLServerPushStreamReader
{
public:
    QnTestCameraStreamReader(
        const QnTestCameraResourcePtr& res);
    virtual ~QnTestCameraStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseStop() override;

    int receiveData(quint8* buffer, int size);
private:
    mutable QnMutex m_socketMutex;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    QnConstMediaContextPtr m_context;
};

#endif // #ifdef ENABLE_TEST_CAMERA
