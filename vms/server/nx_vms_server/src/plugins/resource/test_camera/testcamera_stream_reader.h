#pragma once

#if defined(ENABLE_TEST_CAMERA)

#include <providers/spush_media_stream_provider.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/network/socket.h>
#include <nx/utils/log/log.h>

#include "testcamera_resource.h"

namespace nx::vms::testcamera::packet { class Header; } //< private

class QnTestCameraStreamReader: public CLServerPushStreamReader
{
public:
    QnTestCameraStreamReader(
        const QnTestCameraResourcePtr& res);
    virtual ~QnTestCameraStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired, const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseStop() override;

private:
    bool receiveData(void* buffer, int size, const QString& dataCaptionForErrorMessage);

    template<typename Data, typename Target>
    bool receiveData(Target* target, const QString& dataCaptionForErrorMessage)
    {
        Data data;
        if (!receiveData(&data, sizeof(data), dataCaptionForErrorMessage))
            return false;
        NX_VERBOSE(this, "Received %1: %2", dataCaptionForErrorMessage, data);
        *target = data;
        return true;
    }

    QnAbstractMediaDataPtr receiveFrame(
        const nx::vms::testcamera::packet::Header& header);

private:
    mutable QnMutex m_socketMutex;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    QnConstMediaContextPtr m_context;
};

#endif // defined(ENABLE_TEST_CAMERA)
