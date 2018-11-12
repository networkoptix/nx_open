#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

class QnVistaResource : public QnPlOnvifResource
{
    Q_OBJECT
    typedef QnPlOnvifResource base_type;

public:
    QnVistaResource(QnMediaServerModule* serverModule);
    virtual ~QnVistaResource();

protected:
    virtual QnAbstractPtzController* createPtzControllerInternal() const override;
    virtual void startInputPortStatesMonitoring() override;
};

#endif //ENABLE_ONVIF
