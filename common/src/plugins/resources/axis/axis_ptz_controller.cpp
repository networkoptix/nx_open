#include "axis_ptz_controller.h"

static const quint16 DEFAULT_AXIS_API_PORT = 80; // TODO: #Elric copypasta from axis_resource.cpp

QnAxisPtzController::QnAxisPtzController(const QnPlAxisResourcePtr &resource, QObject *parent):
    QnAbstractPtzController(resource, parent),
    m_resource(resource),
    m_capabilities(Qn::ContinuousPtzCapability) // TODO: #Elric need to check for this?
{
    CLSimpleHTTPClient http(m_resource->getHostAddress(), QUrl(m_resource->getUrl()).port(DEFAULT_AXIS_API_PORT), m_resource->getNetworkTimeout(), m_resource->getAuth());

    // TODO: #Elric hack, need to check other absolute spaces.
    QString absolutePanString;
    m_resource->readAxisParameter(&http, lit("PTZ.Support.S1.AbsolutePan"), &absolutePanString);
    if(absolutePanString == lit("true"))
        m_capabilities |= Qn::AbsolutePtzCapability;
}

QnAxisPtzController::~QnAxisPtzController() {
    return;
}

int QnAxisPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override {
    return 0;
}

int QnAxisPtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos) override {
    return 0;
}

int QnAxisPtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override {
    return 0;
}

int QnAxisPtzController::stopMove() override {
    return 0;
}

Qn::CameraCapabilities QnAxisPtzController::getCapabilities() override {
    return m_capabilities;
}
