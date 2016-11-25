#pragma once 

#ifdef ENABLE_FLIR

#include <boost/optional.hpp>

#include "flir_fc_private.h"
#include "flir_web_socket_io_manager.h"

#include <core/resource/camera_resource.h>
#include <nx/network/http/httpclient.h>

namespace nx {
namespace plugins {
namespace flir {

class FcResource: public QnPhysicalCameraResource
{
    Q_OBJECT
public:
    FcResource();
    virtual ~FcResource();

    virtual CameraDiagnostics::Result initInternal() override;

    virtual bool startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler) override;
    virtual void stopInputPortMonitoringAsync() override;

    virtual QnIOPortDataList getRelayOutputList() const override;
    virtual QnIOPortDataList getInputPortList() const override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int, int) override;
    virtual bool  hasDualStreaming() const override;

private:
    bool doGetRequestAndCheckResponse(nx_http::HttpClient& httpClient, const QUrl& url);
    boost::optional<fc_private::ServerStatus> getNexusServerStatus(nx_http::HttpClient& httpClient);
    bool tryToEnableNexusServer(nx_http::HttpClient& httpClient);

private:
    nexus::WebSocketIoManager* m_ioManager;
};

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // ENABLE_FLIR