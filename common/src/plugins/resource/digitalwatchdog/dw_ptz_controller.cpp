#ifdef ENABLE_ONVIF
#include "dw_ptz_controller.h"

#include "digital_watchdog_resource.h"

static const int HTTP_PORT = 80; // TODO: #Elric copypasta from digital_watchdog_resource.cpp

QnDwPtzController::QnDwPtzController(const QnPlWatchDogResourcePtr &resource):
    base_type(resource),
    m_resource(resource)
{
    CLSimpleHTTPClient http(resource->getHostAddress(), HTTP_PORT, resource->getNetworkTimeout(), resource->getAuth());
    http.doGET(QByteArray("/cgi-bin/getconfig.cgi?action=color"));
    
    QByteArray data;
    http.readAll(data);

    m_flip = 0;
    if(data.contains("flipmode1: 1"))
        m_flip ^= Qt::Vertical | Qt::Horizontal;
    if(data.contains("mirrormode1: 1"))
        m_flip ^= Qt::Horizontal;
}

QnDwPtzController::~QnDwPtzController() {
    return;
}

bool QnDwPtzController::continuousMove(const QVector3D &speed) {
    QVector3D localSpeed = speed;
    
    if(m_flip & Qt::Horizontal)
        localSpeed.setX(-localSpeed.x());
    if(m_flip & Qt::Vertical)
        localSpeed.setY(-localSpeed.y());

    return base_type::continuousMove(localSpeed);
}

bool QnDwPtzController::getFlip(Qt::Orientations *flip) {
    *flip = m_flip;
    return true;
}

#endif // ENABLE_ONVIF
