#include "remote_ptz_controller.h"

#include <QtCore/QEventLoop>

#include <boost/preprocessor/tuple/enum.hpp>

#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>

QnRemotePtzController::QnRemotePtzController(const QnNetworkResourcePtr &resource):
    base_type(resource),
    m_resource(resource),
    m_sequenceId(QnUuid::createUuid()),
    m_sequenceNumber(1)
{
    connect(resource.data(), &QnResource::ptzCapabilitiesChanged, this, [this]{ emit changed(Qn::CapabilitiesPtzField); });
}

QnRemotePtzController::~QnRemotePtzController() {
    return;
}

Qn::PtzCapabilities QnRemotePtzController::getCapabilities() {
    Qn::PtzCapabilities result = m_resource->getPtzCapabilities();
    if(result == Qn::NoPtzCapabilities)
        return Qn::NoPtzCapabilities;

    if(result & Qn::VirtualPtzCapability)
        return Qn::NoPtzCapabilities; /* Can't have remote virtual PTZ. */

    result |= Qn::AsynchronousPtzCapability;
    result &= ~(Qn::FlipPtzCapability | Qn::LimitsPtzCapability);
    return result;
}

QnMediaServerResourcePtr QnRemotePtzController::getMediaServer() const {
    return m_resource->getParentResource().dynamicCast<QnMediaServerResource>();
}

bool QnRemotePtzController::isPointless(Qn::PtzCommand command) {
    if(!getMediaServer())
        return true;

    Qn::ResourceStatus status = m_resource->getStatus();
    if(status == Qn::Unauthorized || status == Qn::Offline)
        return true;

    return !base_type::supports(command);
}

int QnRemotePtzController::nextSequenceNumber() {
    return m_sequenceNumber.fetchAndAddOrdered(1);
}

// TODO: #Elric get rid of this macro hell
#define RUN_COMMAND(COMMAND, RETURN_VALUE, FUNCTION, ... /* PARAMS */)          \
    {                                                                           \
        Qn::PtzCommand command = COMMAND;                                       \
        if(isPointless(command))                                                \
            return false;                                                       \
                                                                                \
        int handle = getMediaServer()->apiConnection()->FUNCTION(m_resource, ##__VA_ARGS__, this, SLOT(at_replyReceived(int, const QVariant &, int))); \
                                                                                \
        SCOPED_MUTEX_LOCK( locker, &m_mutex);                                          \
        m_dataByHandle[handle] = PtzCommandData(command, QVariant::fromValue(RETURN_VALUE)); \
        return true;                                                            \
    }


bool QnRemotePtzController::continuousMove(const QVector3D &speed) {
    RUN_COMMAND(Qn::ContinuousMovePtzCommand, speed, ptzContinuousMoveAsync, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::continuousFocus(qreal speed) {
    RUN_COMMAND(Qn::ContinuousFocusPtzCommand, speed, ptzContinuousFocusAsync, speed);
}

bool QnRemotePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    RUN_COMMAND(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space), position, ptzAbsoluteMoveAsync, space, position, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    RUN_COMMAND(Qn::ViewportMovePtzCommand, viewport, ptzViewportMoveAsync, aspectRatio, viewport, speed, m_sequenceId, nextSequenceNumber());
}

bool QnRemotePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *) {
    RUN_COMMAND(spaceCommand(Qn::GetDevicePositionPtzCommand, space), QVariant(), ptzGetPositionAsync, space);
}

bool QnRemotePtzController::getLimits(Qn::PtzCoordinateSpace, QnPtzLimits *) {
    return false;
}

bool QnRemotePtzController::getFlip(Qt::Orientations *) {
    return false;
}

bool QnRemotePtzController::createPreset(const QnPtzPreset &preset) {
    RUN_COMMAND(Qn::CreatePresetPtzCommand, preset, ptzCreatePresetAsync, preset);
}

bool QnRemotePtzController::updatePreset(const QnPtzPreset &preset) {
    RUN_COMMAND(Qn::UpdatePresetPtzCommand, preset, ptzUpdatePresetAsync, preset);
}

bool QnRemotePtzController::removePreset(const QString &presetId) {
    RUN_COMMAND(Qn::RemovePresetPtzCommand, presetId, ptzRemovePresetAsync, presetId);
}

bool QnRemotePtzController::activatePreset(const QString &presetId, qreal speed) {
    RUN_COMMAND(Qn::ActivatePresetPtzCommand, presetId, ptzActivatePresetAsync, presetId, speed);
}

bool QnRemotePtzController::getPresets(QnPtzPresetList *) {
    RUN_COMMAND(Qn::GetPresetsPtzCommand, QVariant(), ptzGetPresetsAsync);
}

bool QnRemotePtzController::createTour(const QnPtzTour &tour) {
    RUN_COMMAND(Qn::CreateTourPtzCommand, tour, ptzCreateTourAsync, tour);
}

bool QnRemotePtzController::removeTour(const QString &tourId) {
    RUN_COMMAND(Qn::RemoveTourPtzCommand, tourId, ptzRemoveTourAsync, tourId);
}

bool QnRemotePtzController::activateTour(const QString &tourId) {
    RUN_COMMAND(Qn::ActivateTourPtzCommand, tourId, ptzActivateTourAsync, tourId);
}

bool QnRemotePtzController::getTours(QnPtzTourList *) {
    RUN_COMMAND(Qn::GetToursPtzCommand, QVariant(), ptzGetToursAsync);
}

bool QnRemotePtzController::getActiveObject(QnPtzObject *) {
    RUN_COMMAND(Qn::GetActiveObjectPtzCommand, QVariant(), ptzGetActiveObjectAsync);
}

bool QnRemotePtzController::updateHomeObject(const QnPtzObject &homePosition) {
    RUN_COMMAND(Qn::UpdateHomeObjectPtzCommand, homePosition, ptzUpdateHomeObjectAsync, homePosition);
}

bool QnRemotePtzController::getHomeObject(QnPtzObject *) {
    RUN_COMMAND(Qn::GetHomeObjectPtzCommand, QVariant(), ptzGetHomeObjectAsync);
}

bool QnRemotePtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList *auxilaryTraits) {
    Q_UNUSED(auxilaryTraits)
    RUN_COMMAND(Qn::GetAuxilaryTraitsPtzCommand, QVariant(), ptzGetAuxilaryTraitsAsync);
}

bool QnRemotePtzController::runAuxilaryCommand(const QnPtzAuxilaryTrait &trait, const QString &data) {
    RUN_COMMAND(Qn::RunAuxilaryCommandPtzCommand, trait, ptzRunAuxilaryCommandAsync, trait, data);
}

bool QnRemotePtzController::getData(Qn::PtzDataFields query, QnPtzData *) {
    RUN_COMMAND(Qn::GetDataPtzCommand, QVariant(), ptzGetDataAsync, query);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRemotePtzController::at_replyReceived(int status, const QVariant &reply, int handle) {
    PtzCommandData data;
    {
        SCOPED_MUTEX_LOCK( locker, &m_mutex);

        auto pos = m_dataByHandle.find(handle);
        if(pos == m_dataByHandle.end())
            return; /* This really should never happen. */

        data = *pos;
        m_dataByHandle.erase(pos);
    }

    if(status == 0) {
        if(!data.value.isValid())
            data.value = reply;
    } else {
        data.value = QVariant();
    }

    emit finished(data.command, data.value);
}
