#include "remote_ptz_controller.h"

#include <QtCore/QEventLoop>

#include <boost/preprocessor/tuple/enum.hpp>

#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

namespace {
    Qn::PtzCommand spaceCommand(Qn::PtzCommand command, Qn::PtzCoordinateSpace space) {
        switch (command) {
        case Qn::AbsoluteDeviceMovePtzCommand:
        case Qn::AbsoluteLogicalMovePtzCommand:
            return space == Qn::DevicePtzCoordinateSpace ? Qn::AbsoluteDeviceMovePtzCommand : Qn::AbsoluteLogicalMovePtzCommand;
        case Qn::GetDevicePositionPtzCommand:
        case Qn::GetLogicalPositionPtzCommand:
            return space == Qn::DevicePtzCoordinateSpace ? Qn::GetDevicePositionPtzCommand : Qn::GetLogicalPositionPtzCommand;
        case Qn::GetDeviceLimitsPtzCommand:
        case Qn::GetLogicalLimitsPtzCommand:
            return space == Qn::DevicePtzCoordinateSpace ? Qn::GetDeviceLimitsPtzCommand : Qn::GetLogicalLimitsPtzCommand;
        default:
            return command;
        }
    }

} // anonymous namespace

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

#define QN_RUN_COMMAND(COMMAND, FUNCTION, ... /* PARAMS */)                     \
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
    QN_RUN_COMMAND(Qn::ContinuousMovePtzCommand, ptzContinuousMoveAsync, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    QN_RUN_COMMAND(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space), ptzAbsoluteMoveAsync, space, position, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    QN_RUN_COMMAND(Qn::ViewportMovePtzCommand, ptzViewportMoveAsync, aspectRatio, viewport, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *) {
    QN_RUN_COMMAND(spaceCommand(Qn::GetDevicePositionPtzCommand, space), ptzGetPositionAsync, space);
}

bool QnRemotePtzController::getLimits(Qn::PtzCoordinateSpace, QnPtzLimits *) {
    return false;
}

bool QnRemotePtzController::getFlip(Qt::Orientations *) {
    return false;
}

bool QnRemotePtzController::createPreset(const QnPtzPreset &preset) {
    QN_RUN_COMMAND(Qn::CreatePresetPtzCommand, ptzCreatePresetAsync, preset);
}

bool QnRemotePtzController::updatePreset(const QnPtzPreset &preset) {
    QN_RUN_COMMAND(Qn::UpdatePresetPtzCommand, ptzUpdatePresetAsync, preset);
}

bool QnRemotePtzController::removePreset(const QString &presetId) {
    QN_RUN_COMMAND(Qn::RemovePresetPtzCommand, ptzRemovePresetAsync, presetId);
}

bool QnRemotePtzController::activatePreset(const QString &presetId, qreal speed) {
    QN_RUN_COMMAND(Qn::ActivatePresetPtzCommand, ptzActivatePresetAsync, presetId, speed);
}

bool QnRemotePtzController::getPresets(QnPtzPresetList *) {
    QN_RUN_COMMAND(Qn::GetPresetsPtzCommand, ptzGetPresetsAsync);
}

bool QnRemotePtzController::createTour(const QnPtzTour &tour) {
    QN_RUN_COMMAND(Qn::CreateTourPtzCommand, ptzCreateTourAsync, tour);
}

bool QnRemotePtzController::removeTour(const QString &tourId) {
    QN_RUN_COMMAND(Qn::RemoveTourPtzCommand, ptzRemoveTourAsync, tourId);
}

bool QnRemotePtzController::activateTour(const QString &tourId) {
    QN_RUN_COMMAND(Qn::ActivateTourPtzCommand, ptzActivateTourAsync, tourId);
}

bool QnRemotePtzController::getTours(QnPtzTourList *tours) {
    QN_RUN_COMMAND(Qn::GetToursPtzCommand, ptzGetToursAsync);
}

bool QnRemotePtzController::synchronize(Qn::PtzDataFields query) {
    if(query == Qn::NoPtzFields) {
        emit finishedLater(Qn::SynchronizePtzCommand, QVariant()); /* Failure. */
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
        emit synchronized(reply);
}
