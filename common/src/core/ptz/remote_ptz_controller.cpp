#include "remote_ptz_controller.h"

#include <QtCore/QEventLoop>

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

bool QnRemotePtzController::continuousMove(const QVector3D &speed) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzContinuousMoveAsync(m_resource, speed, m_sequenceId, m_sequenceNumber++, this, SLOT(at_continuousMove_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzAbsoluteMoveAsync(m_resource, space, position, m_sequenceId, m_sequenceNumber++, this, SLOT(at_absoluteMove_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::relativeMove(qreal aspectRatio, const QRectF &viewport) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzRelativeMoveAsync(m_resource, aspectRatio, viewport, m_sequenceId, m_sequenceNumber++, this, SLOT(at_relativeMove_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(!m_server)
        return false;

    QnConnectionRequestResult result;
    m_server->apiConnection()->ptzGetPosition(m_resource, space, &result, SLOT(processReply(int, const QVariant &, int)));
    if(result.exec() != 0)
        return false;

    *position = result.reply<QVector3D>();
    return true;
}

bool QnRemotePtzController::getLimits(QnPtzLimits *) {
    return false;
}

bool QnRemotePtzController::getFlip(Qt::Orientations *) {
    return false;
}

bool QnRemotePtzController::createPreset(QnPtzPreset *preset) {
    return false;
}

bool QnRemotePtzController::removePreset(const QnPtzPreset &preset) {
    return false;
}

bool QnRemotePtzController::activatePreset(const QnPtzPreset &preset) {
    return false;
}

bool QnRemotePtzController::getPresets(QnPtzPresetList *presets) {
    return false;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRemotePtzController::at_continuousMove_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_absoluteMove_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_relativeMove_replyReceived(int status, int handle) {
    return;
}

