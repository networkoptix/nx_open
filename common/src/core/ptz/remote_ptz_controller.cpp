#include "remote_ptz_controller.h"

#include <QtCore/QEventLoop>

#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

QnRemotePtzController::QnRemotePtzController(const QnNetworkResourcePtr &resource):
    base_type(resource),
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

    connect(resource.data(), SIGNAL(ptzCapabilitiesChanged(const QnResourcePtr &)), this, SIGNAL(capabilitiesChanged()));
    connect(this, SIGNAL(synchronizedLater(Qn::PtzDataFields)), this, SIGNAL(synchronized(Qn::PtzDataFields)), Qt::QueuedConnection);

    synchronize(Qn::AllPtzFields);
}

QnRemotePtzController::~QnRemotePtzController() {
    return;
}

Qn::PtzCapabilities QnRemotePtzController::getCapabilities() {
    Qn::PtzCapabilities result = m_resource->getPtzCapabilities();
    result |= Qn::NonBlockingPtzCapability;
    result &= ~(Qn::FlipPtzCapability | Qn::LimitsPtzCapability);
    return result;
}

bool QnRemotePtzController::continuousMove(const QVector3D &speed) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzContinuousMoveAsync(m_resource, speed, m_sequenceId, m_sequenceNumber++, this, SLOT(at_continuousMove_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzAbsoluteMoveAsync(m_resource, space, position, speed, m_sequenceId, m_sequenceNumber++, this, SLOT(at_absoluteMove_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzViewportMoveAsync(m_resource, aspectRatio, viewport, speed, m_sequenceId, m_sequenceNumber++, this, SLOT(at_relativeMove_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(!m_server)
        return false;

    QnConnectionRequestResult result;
    m_server->apiConnection()->ptzGetPositionAsync(m_resource, space, &result, SLOT(processReply(int, const QVariant &, int)));
    if(result.exec() != 0)
        return false;

    *position = result.reply<QVector3D>();
    return true;
}

bool QnRemotePtzController::getLimits(Qn::PtzCoordinateSpace, QnPtzLimits *) {
    return false;
}

bool QnRemotePtzController::getFlip(Qt::Orientations *) {
    return false;
}

bool QnRemotePtzController::createPreset(const QnPtzPreset &preset) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzCreatePresetAsync(m_resource, preset, this, SLOT(at_createPreset_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::updatePreset(const QnPtzPreset &preset) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzUpdatePresetAsync(m_resource, preset, this, SLOT(at_updatePreset_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::removePreset(const QString &presetId) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzRemovePresetAsync(m_resource, presetId, this, SLOT(at_removePreset_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::activatePreset(const QString &presetId, qreal speed) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzActivatePresetAsync(m_resource, presetId, speed, this, SLOT(at_activatePreset_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::getPresets(QnPtzPresetList *presets) {
    if(!m_server)
        return false;

    *presets = m_data.presets; // TODO: #Elric implement properly =)

    /*QnConnectionRequestResult result;
    m_server->apiConnection()->ptzGetPresetsAsync(m_resource, &result, SLOT(processReply(int, const QVariant &, int)));
    if(result.exec() != 0)
        return false;

    *presets = result.reply<QnPtzPresetList>();*/
    return true;
}

bool QnRemotePtzController::createTour(const QnPtzTour &tour) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzCreateTourAsync(m_resource, tour, this, SLOT(at_createTour_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::removeTour(const QString &tourId) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzRemoveTourAsync(m_resource, tourId, this, SLOT(at_removeTour_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::activateTour(const QString &tourId) {
    if(!m_server)
        return false;

    m_server->apiConnection()->ptzActivateTourAsync(m_resource, tourId, this, SLOT(at_activateTour_replyReceived(int, int)));
    return true;
}

bool QnRemotePtzController::getTours(QnPtzTourList *tours) {
    if(!m_server)
        return false;

    *tours = m_data.tours; // TODO: #Elric implement properly =)
    return true;
}

void QnRemotePtzController::synchronize(Qn::PtzDataFields fields) {
    if(fields == Qn::NoPtzFields) {
        emit synchronizedLater(fields);
        return;
    }

    int handle = m_server->apiConnection()->ptzGetDataAsync(m_resource, fields, this, SLOT(at_getData_replyReceived(int, const QnPtzData &, int)));
    m_fieldsByHandle.insert(handle, fields);
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

void QnRemotePtzController::at_createPreset_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_updatePreset_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_removePreset_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_activatePreset_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_createTour_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_removeTour_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_activateTour_replyReceived(int status, int handle) {
    return;
}

void QnRemotePtzController::at_getData_replyReceived(int status, const QnPtzData &reply, int handle) {
    Qn::PtzDataFields fields = m_fieldsByHandle.value(handle, Qn::NoPtzFields);
    m_fieldsByHandle.remove(handle);
    
    // TODO: store only valid fields!
    m_data = reply;
    
    if(fields != Qn::NoPtzFields)
        emit synchronized(fields);
}


