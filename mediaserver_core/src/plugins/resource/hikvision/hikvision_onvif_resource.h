#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

class QnHikvisionOnvifResource: public QnPlOnvifResource
{
    Q_OBJECT
public:
    QnHikvisionOnvifResource();
protected:
    virtual CameraDiagnostics::Result initInternal() override;
private:
    CameraDiagnostics::Result initialize2WayAudio();
};

typedef QnSharedResourcePointer<QnHikvisionOnvifResource> QnHikvisionOnvifResourcePtr;

#endif  //ENABLE_ONVIF
