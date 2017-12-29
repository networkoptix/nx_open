#pragma once 

#ifdef ENABLE_FLIR

#include <boost/optional.hpp>

#include "flir_fc_private.h"
#include "flir_web_socket_io_manager.h"

#include <core/resource/camera_resource.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace plugins {
namespace flir {

/**
 * Flir FC-series resource.
 */
class FcResource: public QnPhysicalCameraResource
{
    Q_OBJECT

    struct PortTimerEntry
    {
        QString portId;
        bool state;
    };

public:
    FcResource();
    virtual ~FcResource();

    virtual CameraDiagnostics::Result initInternal() override;

    virtual bool startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler) override;
    virtual void stopInputPortMonitoringAsync() override;

    virtual QnIOPortDataList getRelayOutputList() const override;
    virtual QnIOPortDataList getInputPortList() const override;

    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int, int) override;
    virtual bool  hasDualStreaming() const override;

private:
    bool doGetRequestAndCheckResponse(nx::network::http::HttpClient& httpClient, const nx::utils::Url& url);
    boost::optional<fc_private::ServerStatus> getNexusServerStatus(nx::network::http::HttpClient& httpClient);
    bool tryToEnableNexusServer(nx::network::http::HttpClient& httpClient);

private:
    nexus::WebSocketIoManager* m_ioManager;
    std::map<quint64, PortTimerEntry> m_autoResetTimers;
    bool m_callbackIsInProgress;
    QnMutex m_ioMutex;
    QnWaitCondition m_ioWaitCondition;
};

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // ENABLE_FLIR
