#include "flir_onvif_resource.h"

#ifdef ENABLE_ONVIF

QnFlirOnvifResource::QnFlirOnvifResource()
{
}

CameraDiagnostics::Result QnFlirOnvifResource::initInternal()
{
    QnPlOnvifResource::initInternal();

    /*fetchAndSetAdvancedParameters();
    saveParams();*/

    return CameraDiagnostics::NoErrorResult();
}

#endif // ENABLE_ONVIF
