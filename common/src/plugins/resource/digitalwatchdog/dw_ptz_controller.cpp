#ifdef ENABLE_ONVIF
#include "dw_ptz_controller.h"

#include "digital_watchdog_resource.h"

static const int HTTP_PORT = 80; // TODO: #Elric copypasta from digital_watchdog_resource.cpp

QnDwPtzController::QnDwPtzController(const QnPlWatchDogResourcePtr &resource):
    base_type(resource),
    m_resource(resource)
{
    connect(resource.data(), &QnPlWatchDogResource::physicalParamChanged, this, &QnDwPtzController::at_physicalParamChanged);
    updateFlipState();
}

void QnDwPtzController::updateFlipState()
{
    CLSimpleHTTPClient http(m_resource->getHostAddress(), HTTP_PORT, m_resource->getNetworkTimeout(), m_resource->getAuth());
    http.doGET(QByteArray("/cgi-bin/getconfig.cgi?action=color"));

    QByteArray data;
    http.readAll(data);

    Qt::Orientations flip = 0;
    if(data.contains("flipmode1: 1"))
        flip ^= Qt::Vertical | Qt::Horizontal;
    if(data.contains("mirrormode1: 1"))
        flip ^= Qt::Horizontal;
    m_flip = flip;
}

void QnDwPtzController::at_physicalParamChanged(const QString& name, const QString& value)
{
    Q_UNUSED(value)
    if (name == lit("Flip") || name == lit("Mirror"))
        QTimer::singleShot(500, this, SLOT(updateFlipState())); // DW cameras doesn't show actual settings if read it immediatly
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
