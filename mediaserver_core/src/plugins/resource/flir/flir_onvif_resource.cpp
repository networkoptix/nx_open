#include "flir_onvif_resource.h"

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
