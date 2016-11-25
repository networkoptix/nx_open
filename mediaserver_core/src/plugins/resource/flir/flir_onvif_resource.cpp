#include "flir_onvif_resource.h"

#ifdef ENABLE_ONVIF

using namespace nx::plugins;

flir::OnvifResource::OnvifResource()
{
}

CameraDiagnostics::Result flir::OnvifResource::initInternal()
{
    return QnPlOnvifResource::initInternal();
}

#endif // ENABLE_ONVIF
