#include "flir_onvif_resource.h"

#ifdef ENABLE_ONVIF

namespace nx {
namespace plugins {
namespace flir {

OnvifResource::OnvifResource()
{
}

CameraDiagnostics::Result OnvifResource::initInternal()
{
    return QnPlOnvifResource::initInternal();
}

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // ENABLE_ONVIF
