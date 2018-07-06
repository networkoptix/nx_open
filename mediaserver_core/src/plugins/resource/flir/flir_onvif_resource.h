#pragma once

#if defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)

#include <plugins/resource/onvif/onvif_resource.h>

namespace nx {
namespace plugins {
namespace flir {

class OnvifResource: public QnPlOnvifResource
{
    Q_OBJECT
public:
    OnvifResource();
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
};

} // namespace flir
} // namespace plugins
} // namespace nx

typedef QnSharedResourcePointer<nx::plugins::flir::OnvifResource> QnFlirOnvifResourcePtr;
#endif //  ENABLE_ONVIF
