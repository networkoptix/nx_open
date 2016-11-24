#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

#include "flir_web_socket_io_manager.h"

namespace nx {
namespace plugins {
namespace flir {

class OnvifResource: public QnPlOnvifResource
{
    Q_OBJECT

public:
    OnvifResource();
    virtual ~OnvifResource();

    virtual CameraDiagnostics::Result initInternal() override;
    virtual bool startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler) override;
    virtual void stopInputPortMonitoringAsync() override;
    virtual QnIOPortDataList getRelayOutputList() const override;
    virtual QnIOPortDataList getInputPortList() const override;

    virtual bool setRelayOutputState(
        const QString& outputID,
        bool isActive,
        unsigned int autoResetTimeoutMS) override;

private:
    nx::plugins::flir::nexus::WebSocketIoManager* m_ioManager;
};

typedef QnSharedResourcePointer<OnvifResource> QnFlirOnvifResourcePtr;

} // namespace flir
} // namespace plugins
} // namespace nx

#endif //  ENABLE_ONVIF
