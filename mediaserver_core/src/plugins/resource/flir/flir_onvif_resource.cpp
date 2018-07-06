#include "flir_onvif_resource.h"

#if defined(ENABLE_ONVIF) && defined(ENABLE_FLIR)

namespace nx {
namespace plugins {
namespace flir {

OnvifResource::OnvifResource()
{
}

CameraDiagnostics::Result OnvifResource::initializeCameraDriver()
{
    return QnPlOnvifResource::initializeCameraDriver();
}

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // ENABLE_ONVIF
