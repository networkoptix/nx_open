#include "flir_onvif_resource.h"
#include <QtWebSockets/QWebSocket>

#ifdef ENABLE_ONVIF

QnFlirOnvifResource::QnFlirOnvifResource()
{
}

CameraDiagnostics::Result QnFlirOnvifResource::initInternal()
{
    return QnPlOnvifResource::initInternal();
}

bool QnFlirOnvifResource::startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler)
{
    m_ioManager->startIOMonitoring();
    return true;
}

void QnFlirOnvifResource::stopInputPortMonitoringAsync()
{
    m_ioManager->stopIOMonitoring();
}

#endif // ENABLE_ONVIF
