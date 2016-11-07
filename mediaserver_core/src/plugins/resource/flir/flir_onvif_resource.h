#ifndef __FLIR_ONVIF_RESOURCE_H__
#define __FLIR_ONVIF_RESOURCE_H__

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

#include "flir_ws_io_manager.h"

class QnFlirOnvifResource : public QnPlOnvifResource
{
    Q_OBJECT

public:
    QnFlirOnvifResource();
    virtual ~QnFlirOnvifResource();

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
    FlirWebSocketIoManager* m_ioManager;
};

typedef QnSharedResourcePointer<QnFlirOnvifResource> QnFlirOnvifResourcePtr;

#endif //  ENABLE_FLIR
#endif //__FLIR_ONVIF_RESOURCE_H__
