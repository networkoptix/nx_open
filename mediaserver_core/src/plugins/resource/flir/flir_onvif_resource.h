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
    virtual CameraDiagnostics::Result initInternal() override;
};

typedef QnSharedResourcePointer<OnvifResource> QnFlirOnvifResourcePtr;

} // namespace flir
} // namespace plugins
} // namespace nx

#endif //  ENABLE_ONVIF