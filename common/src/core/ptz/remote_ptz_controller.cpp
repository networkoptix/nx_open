#include "remote_ptz_controller.h"

#include <cassert>

#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

QnRemotePtzController::QnRemotePtzController(const QnNetworkResourcePtr &resource):
    QnAbstractPtzController(resource),
    m_resource(resource)
{
    m_sequenceId = QUuid::createUuid();
    m_sequenceNumber = 0;

    m_server = m_resource->getParentResource().dynamicCast<QnMediaServerResource>();
    if(!m_server) {
        /* Apparently this really does happen... */
        qnWarning("No parent server for network resource '%1'.", resource->getName());
        return;
    }
}

QnRemotePtzController::~QnRemotePtzController() {
    return;
}

Qn::PtzCapabilities QnRemotePtzController::getCapabilities() {
    return m_resource->getPtzCapabilities();
}

int QnRemotePtzController::continuousMove(const QVector3D &speed) {
    if(!m_server)
        return 1;

    m_server->apiConnection()->ptzContinuousMoveAsync(m_resource, speed, m_sequenceId, m_sequenceNumber++, this, SLOT(at_continuousMove_replyReceived(int, int)));
    return 0;
}

int QnRemotePtzController::absoluteMove(const QVector3D &position) {
    return 1;
}

int QnRemotePtzController::relativeMove(qreal aspectRatio, const QRectF &viewport) {
    return 1;
}

int QnRemotePtzController::getPosition(QVector3D *) {
    return 1;
}

int QnRemotePtzController::getLimits(QnPtzLimits *) {
    return 1;
}

int QnRemotePtzController::getFlip(Qt::Orientations *) {
    return 1;
}

void QnRemotePtzController::at_continuousMove_replyReceived(int status, int handle) {
    return;
}

