#include "remote_ptz_controller.h"

#include <QtCore/QEventLoop>

#include <boost/preprocessor/tuple/enum.hpp>

#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

QnRemotePtzController::QnRemotePtzController(const QnNetworkResourcePtr &resource):
    base_type(resource),
    m_resource(resource),
    m_sequenceId(QUuid::createUuid()),
    m_sequenceNumber(1)
{
    m_server = m_resource->getParentResource().dynamicCast<QnMediaServerResource>();
    if(!m_server) {
        /* Apparently this really does happen... */
        qnWarning("No parent server for network resource '%1'.", resource->getName());
        return;
    }

    connect(resource.data(), &QnResource::ptzCapabilitiesChanged, this, &QnAbstractPtzController::capabilitiesChanged);
    connect(this, &QnRemotePtzController::finishedLater, this, &QnAbstractPtzController::finished, Qt::QueuedConnection);

    synchronize(Qn::AllPtzFields);
}

QnRemotePtzController::~QnRemotePtzController() {
    return;
}

Qn::PtzCapabilities QnRemotePtzController::getCapabilities() {
    Qn::PtzCapabilities result = m_resource->getPtzCapabilities();
    result |= Qn::AsynchronousPtzCapability;
    result &= ~(Qn::FlipPtzCapability | Qn::LimitsPtzCapability);
    return result;
}

bool QnRemotePtzController::isPointless(Qn::PtzCommand command) {
    if(!m_server)
        return true;

    QnResource::Status status = m_resource->getStatus();
    if(status == QnResource::Unauthorized || status == QnResource::Offline)
        return true;

    return !base_type::supports(command);
}

int QnRemotePtzController::nextSequenceNumber() {
    return m_sequenceNumber.fetchAndAddOrdered(1);
}

#define RUN_COMMAND(COMMAND, FUNCTION, ... /* PARAMS */)                        \
    {                                                                           \
        Qn::PtzCommand command = COMMAND;                                       \
        if(isPointless(command))                                                \
            return false;                                                       \
                                                                                \
        int handle = m_server->apiConnection()->FUNCTION(m_resource, ##__VA_ARGS__, this, SLOT(at_replyReceived(int, const QVariant &, int))); \
                                                                                \
        QMutexLocker locker(&m_mutex);                                          \
        m_commandByHandle[handle] = command;                                    \
        return true;                                                            \
    }


bool QnRemotePtzController::continuousMove(const QVector3D &speed) {
    RUN_COMMAND(Qn::ContinuousMovePtzCommand, ptzContinuousMoveAsync, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    RUN_COMMAND(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space), ptzAbsoluteMoveAsync, space, position, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    RUN_COMMAND(Qn::ViewportMovePtzCommand, ptzViewportMoveAsync, aspectRatio, viewport, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *) {
    RUN_COMMAND(spaceCommand(Qn::GetDevicePositionPtzCommand, space), ptzGetPositionAsync, space);
}

bool QnRemotePtzController::getLimits(Qn::PtzCoordinateSpace, QnPtzLimits *) {
    return false;
}

bool QnRemotePtzController::getFlip(Qt::Orientations *) {
    return false;
}

bool QnRemotePtzController::createPreset(const QnPtzPreset &preset) {
    RUN_COMMAND(Qn::CreatePresetPtzCommand, ptzCreatePresetAsync, preset);
}

bool QnRemotePtzController::updatePreset(const QnPtzPreset &preset) {
    RUN_COMMAND(Qn::UpdatePresetPtzCommand, ptzUpdatePresetAsync, preset);
}

bool QnRemotePtzController::removePreset(const QString &presetId) {
    RUN_COMMAND(Qn::RemovePresetPtzCommand, ptzRemovePresetAsync, presetId);
}

bool QnRemotePtzController::activatePreset(const QString &presetId, qreal speed) {
    RUN_COMMAND(Qn::ActivatePresetPtzCommand, ptzActivatePresetAsync, presetId, speed);
}

bool QnRemotePtzController::getPresets(QnPtzPresetList *) {
    RUN_COMMAND(Qn::GetPresetsPtzCommand, ptzGetPresetsAsync);
}

bool QnRemotePtzController::createTour(const QnPtzTour &tour) {
    RUN_COMMAND(Qn::CreateTourPtzCommand, ptzCreateTourAsync, tour);
}

bool QnRemotePtzController::removeTour(const QString &tourId) {
    RUN_COMMAND(Qn::RemoveTourPtzCommand, ptzRemoveTourAsync, tourId);
}

bool QnRemotePtzController::activateTour(const QString &tourId) {
    RUN_COMMAND(Qn::ActivateTourPtzCommand, ptzActivateTourAsync, tourId);
}

bool QnRemotePtzController::getTours(QnPtzTourList *tours) {
    RUN_COMMAND(Qn::GetToursPtzCommand, ptzGetToursAsync);
}

bool QnRemotePtzController::synchronize(Qn::PtzDataFields) {
    return true; /* There really is nothing to synchronize. */
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRemotePtzController::at_replyReceived(int status, const QVariant &reply, int handle) {
    Qn::PtzCommand command;
    {
        QMutexLocker locker(&m_mutex);

        auto pos = m_commandByHandle.find(handle);
        if(pos == m_commandByHandle.end())
            return; /* This really should never happen. */

        command = *pos;
        m_commandByHandle.erase(pos);
    }

    QVariant data;
    if(status == 0) {
        data = reply;
        if(!data.isValid())
            data == QVariant(true);
    }

    emit finished(command, data);
}
