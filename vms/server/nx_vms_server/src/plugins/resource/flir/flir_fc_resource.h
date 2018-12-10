#pragma once

#ifdef ENABLE_FLIR

#include <boost/optional.hpp>

#include "flir_fc_private.h"
#include "flir_web_socket_io_manager.h"

#include <nx/vms/server/resource/camera.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace plugins {
namespace flir {

/**
 * Flir FC-series resource.
 */
class FcResource: public nx::vms::server::resource::Camera
{
    Q_OBJECT

    struct PortTimerEntry
    {
        QString portId;
        bool state;
    };

public:
    FcResource(QnMediaServerModule* serverModule);
    virtual ~FcResource();

    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

    virtual bool setOutputPortState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int, int) override;
    virtual bool  hasDualStreamingInternal() const override;

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
