#include "remote_ptz_controller.h"

#include <QtCore/QEventLoop>

#include <boost/preprocessor/tuple/enum.hpp>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

using namespace nx::core;

QnRemotePtzController::QnRemotePtzController(const QnVirtualCameraResourcePtr& camera):
    base_type(camera),
    m_camera(camera),
    m_sequenceId(QnUuid::createUuid()),
    m_sequenceNumber(1)
{
}

QnRemotePtzController::~QnRemotePtzController()
{
}

Ptz::Capabilities QnRemotePtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    Ptz::Capabilities result = m_camera->getPtzCapabilities(options.type);
    if (!result)
        return Ptz::NoPtzCapabilities;

    if (result.testFlag(Ptz::VirtualPtzCapability))
        return Ptz::NoPtzCapabilities; /* Can't have remote virtual PTZ. */

    result |= Ptz::AsynchronousPtzCapability;
    result &= ~(Ptz::FlipPtzCapability | Ptz::LimitsPtzCapability);
    return result;
}

QnMediaServerResourcePtr QnRemotePtzController::getMediaServer() const
{
    return m_camera->getParentServer();
}

bool QnRemotePtzController::isPointless(
    Qn::PtzCommand command,
    const nx::core::ptz::Options& options)
{
    if (!getMediaServer())
        return true;

    const Qn::ResourceStatus status = m_camera->getStatus();
    if (status == Qn::Unauthorized || status == Qn::Offline)
        return true;

    return !base_type::supports(command, options);
}

int QnRemotePtzController::nextSequenceNumber()
{
    return m_sequenceNumber.fetchAndAddOrdered(1);
}

// TODO: #Elric get rid of this macro hell
#define RUN_COMMAND(COMMAND, RETURN_VALUE, FUNCTION, OPTIONS, ... /* PARAMS */) \
    {                                                                           \
        const auto nonConstThis = const_cast<QnRemotePtzController*>(this);     \
        const Qn::PtzCommand command = COMMAND;                                 \
        const nx::core::ptz::Options internalOptions = OPTIONS;                 \
        if (nonConstThis->isPointless(command, internalOptions))                \
            return false;                                                       \
                                                                                \
        auto server = getMediaServer();                                         \
        if (!server)                                                            \
            return false;                                                       \
                                                                                \
        int handle = server->apiConnection()->FUNCTION(                         \
            m_camera, ##__VA_ARGS__, nonConstThis,                            \
                SLOT(at_replyReceived(int, const QVariant &, int)));            \
                                                                                \
        const QnMutexLocker locker(&m_mutex);                                   \
        nonConstThis->m_dataByHandle[handle] =                                  \
            PtzCommandData(command, QVariant::fromValue(RETURN_VALUE));         \
        return true;                                                            \
    }


bool QnRemotePtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(
        Qn::ContinuousMovePtzCommand,
        speed,
        ptzContinuousMoveAsync,
        options,
        speed,
        options,
        m_sequenceId,
        nextSequenceNumber());
}

bool QnRemotePtzController::continuousFocus(
    qreal speed,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(
        Qn::ContinuousFocusPtzCommand,
        speed,
        ptzContinuousFocusAsync,
        options,
        speed,
        options);
}

bool QnRemotePtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(
        spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space),
        position,
        ptzAbsoluteMoveAsync,
        options,
        space,
        position,
        speed,
        options,
        m_sequenceId,
        nextSequenceNumber());
}

bool QnRemotePtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(
        Qn::ViewportMovePtzCommand,
        viewport,
        ptzViewportMoveAsync,
        options,
        aspectRatio,
        viewport,
        speed,
        options,
        m_sequenceId,
        nextSequenceNumber());
}

bool QnRemotePtzController::relativeMove(
    const ptz::Vector& direction,
    const ptz::Options& options)
{
    RUN_COMMAND(
        Qn::RelativeMovePtzCommand,
        direction,
        ptzRelativeMoveAsync,
        options,
        direction,
        options,
        m_sequenceId,
        nextSequenceNumber());
}

bool QnRemotePtzController::relativeFocus(qreal direction, const ptz::Options& options)
{
    RUN_COMMAND(
        Qn::RelativeFocusPtzCommand,
        direction,
        ptzRelativeFocusAsync,
        options,
        direction,
        options);
}

bool QnRemotePtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* /*position*/,
    const nx::core::ptz::Options& options) const
{
    RUN_COMMAND(
        spaceCommand(Qn::GetDevicePositionPtzCommand, space),
        QVariant(),
        ptzGetPositionAsync,
        options,
        space,
        options);
}

bool QnRemotePtzController::getLimits(
    Qn::PtzCoordinateSpace,
    QnPtzLimits* /*limits*/,
    const nx::core::ptz::Options& options) const
{
    return false;
}

bool QnRemotePtzController::getFlip(
    Qt::Orientations* /*flip*/,
    const nx::core::ptz::Options& options) const
{
    return false;
}

bool QnRemotePtzController::createPreset(const QnPtzPreset& preset)
{
    RUN_COMMAND(
        Qn::CreatePresetPtzCommand,
        preset,
        ptzCreatePresetAsync,
        {nx::core::ptz::Type::operational},
        preset);
}

bool QnRemotePtzController::updatePreset(const QnPtzPreset& preset)
{
    RUN_COMMAND(
        Qn::UpdatePresetPtzCommand,
        preset,
        ptzUpdatePresetAsync,
        {nx::core::ptz::Type::operational},
        preset);
}

bool QnRemotePtzController::removePreset(const QString& presetId)
{
    RUN_COMMAND(
        Qn::RemovePresetPtzCommand,
        presetId,
        ptzRemovePresetAsync,
        {nx::core::ptz::Type::operational},
        presetId);
}

bool QnRemotePtzController::activatePreset(const QString& presetId, qreal speed)
{
    RUN_COMMAND(
        Qn::ActivatePresetPtzCommand,
        presetId,
        ptzActivatePresetAsync,
        {nx::core::ptz::Type::operational},
        presetId,
        speed);
}

bool QnRemotePtzController::getPresets(QnPtzPresetList* /*presets*/) const
{
    RUN_COMMAND(
        Qn::GetPresetsPtzCommand,
        QVariant(),
        ptzGetPresetsAsync,
        {nx::core::ptz::Type::operational});
}

bool QnRemotePtzController::createTour(
    const QnPtzTour& tour)
{
    RUN_COMMAND(
        Qn::CreateTourPtzCommand,
        tour,
        ptzCreateTourAsync,
        {nx::core::ptz::Type::operational},
        tour);
}

bool QnRemotePtzController::removeTour(const QString& tourId)
{
    RUN_COMMAND(
        Qn::RemoveTourPtzCommand,
        tourId,
        ptzRemoveTourAsync,
        {nx::core::ptz::Type::operational},
        tourId);
}

bool QnRemotePtzController::activateTour(const QString& tourId)
{
    RUN_COMMAND(
        Qn::ActivateTourPtzCommand,
        tourId,
        ptzActivateTourAsync,
        {nx::core::ptz::Type::operational},
        tourId);
}

bool QnRemotePtzController::getTours(QnPtzTourList* /*tours*/) const
{
    RUN_COMMAND(
        Qn::GetToursPtzCommand,
        QVariant(),
        ptzGetToursAsync,
        {nx::core::ptz::Type::operational});
}

bool QnRemotePtzController::getActiveObject(QnPtzObject* /*object*/) const
{
    RUN_COMMAND(
        Qn::GetActiveObjectPtzCommand,
        QVariant(),
        ptzGetActiveObjectAsync,
        {nx::core::ptz::Type::operational});
}

bool QnRemotePtzController::updateHomeObject(const QnPtzObject& homePosition)
{
    RUN_COMMAND(
        Qn::UpdateHomeObjectPtzCommand,
        homePosition,
        ptzUpdateHomeObjectAsync,
        {nx::core::ptz::Type::operational},
        homePosition);
}

bool QnRemotePtzController::getHomeObject(QnPtzObject* /*object*/) const
{
    RUN_COMMAND(
        Qn::GetHomeObjectPtzCommand,
        QVariant(),
        ptzGetHomeObjectAsync,
        {nx::core::ptz::Type::operational});
}

bool QnRemotePtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* /*auxiliaryTraits*/,
    const nx::core::ptz::Options& options) const
{
    RUN_COMMAND(
        Qn::GetAuxiliaryTraitsPtzCommand,
        QVariant(),
        ptzGetAuxiliaryTraitsAsync,
        options,
        options);
}

bool QnRemotePtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(
        Qn::RunAuxiliaryCommandPtzCommand,
        trait,
        ptzRunAuxiliaryCommandAsync,
        options,
        trait,
        data,
        options);
}

bool QnRemotePtzController::getData(
    Qn::PtzDataFields query,
    QnPtzData* /*data*/,
    const nx::core::ptz::Options& options) const
{
    RUN_COMMAND(
        Qn::GetDataPtzCommand,
        QVariant(),
        ptzGetDataAsync,
        options,
        query,
        options);
}

void QnRemotePtzController::at_replyReceived(int status, const QVariant& reply, int handle)
{
    PtzCommandData data;
    {
        const QnMutexLocker locker(&m_mutex);

        const auto pos = m_dataByHandle.find(handle);
        if (pos == m_dataByHandle.end())
            return; /* This really should never happen. */

        data = *pos;
        m_dataByHandle.erase(pos);
    }

    if (status != 0)
        data.value = QVariant();
    else if (!data.value.isValid())
        data.value = reply;

    emit finished(data.command, data.value);
}
