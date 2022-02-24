// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "threaded_ptz_controller.h"

#include <QtCore/QThreadPool>

#include <common/common_meta_types.h>
#include <core/ptz/ptz_controller_pool.h>
#include <nx/utils/thread/mutex.h>

using namespace nx::vms::common::ptz;

namespace {

using PtzCommandFunctor = std::function<QVariant(const QnPtzControllerPtr& controller)>;
class AsyncPtzCommandExecutor: public AsyncPtzCommandExecutorInterface, public QRunnable
{
public:
    AsyncPtzCommandExecutor(
        const QnPtzControllerPtr& controller,
        Command command,
        const PtzCommandFunctor& functor)
        :
        m_controller(controller),
        m_command(command),
        m_functor(functor)
    {
    }

    virtual void run() override
    {
        const QVariant result = m_functor(m_controller);
        emit finished(m_command, result);
    }

private:
    QnPtzControllerPtr m_controller;
    Command m_command;
    PtzCommandFunctor m_functor;
};

} // namespace

QnThreadedPtzController::QnThreadedPtzController(
    const QnPtzControllerPtr& baseController,
    QThreadPool* threadPool)
    :
    base_type(baseController),
    m_threadPool(threadPool)
{
}

QnThreadedPtzController::~QnThreadedPtzController()
{
}

bool QnThreadedPtzController::extends(Ptz::Capabilities capabilities)
{
    return !capabilities.testFlag(Ptz::AsynchronousPtzCapability);
}

void QnThreadedPtzController::callThreaded(
    Command command,
    const PtzCommandFunctor& functor) const
{
    auto runnable = new AsyncPtzCommandExecutor(baseController(), command, functor);
    runnable->setAutoDelete(true);
    connect(
        runnable,
        &AsyncPtzCommandExecutorInterface::finished,
        this,
        &QnAbstractPtzController::finished,
        Qt::QueuedConnection);

    m_threadPool->start(runnable);
}

template<typename ResultValue, typename MethodPointer, typename... Args>
bool QnThreadedPtzController::executeCommand(
    Command command,
    ResultValue result,
    MethodPointer method,
    Options internalOptions,
    Args&&... args) const
{
    const auto nonConstThis = const_cast<QnThreadedPtzController*>(this);
    if (!nonConstThis->supports(command, internalOptions))
        return false;

    callThreaded(command,
        [=](const QnPtzControllerPtr& controller) -> QVariant
        {
            if (std::invoke(method, *controller.get(), args...))
                return QVariant::fromValue(result);
            return QVariant();
        }
    );

    return true;
}

template<typename ResultType, typename MethodPointer, typename... Args>
bool QnThreadedPtzController::requestData(
    Command command,
    MethodPointer method,
    Options internalOptions,
    Args&&... args) const
{
    const auto nonConstThis = const_cast<QnThreadedPtzController*>(this);
    if (!nonConstThis->supports(command, internalOptions))
        return false;

    callThreaded(command,
        [=](const QnPtzControllerPtr& controller) -> QVariant
        {
            ResultType result;
            if (std::invoke(method, *controller.get(), &result, args...))
                return QVariant::fromValue(result);
            return QVariant();
        }
    );

    return true;
}

Ptz::Capabilities QnThreadedPtzController::getCapabilities(
    const Options& options) const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    return extends(capabilities) ? (capabilities | Ptz::AsynchronousPtzCapability) : capabilities;
}

bool QnThreadedPtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    return executeCommand(
        Command::continuousMove,
        speed,
        &QnAbstractPtzController::continuousMove,
        options,
        speed,
        options);
}

bool QnThreadedPtzController::continuousFocus(
    qreal speed,
    const Options& options)
{
    return executeCommand(
        Command::continuousFocus,
        speed,
        &QnAbstractPtzController::continuousFocus,
        options,
        speed,
        options);
}

bool QnThreadedPtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    return executeCommand(
        spaceCommand(Command::absoluteDeviceMove, space),
        position,
        &QnAbstractPtzController::absoluteMove,
        options,
        space,
        position,
        speed,
        options);
}

bool QnThreadedPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    return executeCommand(
        Command::viewportMove,
        viewport,
        &QnAbstractPtzController::viewportMove,
        options,
        aspectRatio,
        viewport,
        speed,
        options);
}

bool QnThreadedPtzController::relativeMove(
    const Vector& direction,
    const Options& options)
{
    return executeCommand(
        Command::relativeMove,
        direction,
        &QnAbstractPtzController::relativeMove,
        options,
        direction,
        options);
}

bool QnThreadedPtzController::relativeFocus(qreal direction, const Options& options)
{
    return executeCommand(
        Command::relativeFocus,
        direction,
        &QnAbstractPtzController::relativeFocus,
        options,
        direction,
        options);
}

bool QnThreadedPtzController::getPosition(
    Vector* /*position*/,
    CoordinateSpace space,
    const Options& options) const
{
    return requestData<Vector>(
        spaceCommand(Command::getDevicePosition, space),
        &QnAbstractPtzController::getPosition,
        options,
        space,
        options);
}

bool QnThreadedPtzController::getLimits(
    QnPtzLimits* /*limits*/,
    CoordinateSpace space,
    const Options& options) const
{
    return requestData<QnPtzLimits>(
        spaceCommand(Command::getDeviceLimits, space),
        &QnAbstractPtzController::getLimits,
        options,
        space,
        options);
}

bool QnThreadedPtzController::getFlip(
    Qt::Orientations* /*flip*/,
    const Options& options) const
{
    return requestData<Qt::Orientations>(
        Command::getFlip,
        &QnAbstractPtzController::getFlip,
        options,
        options);
}

bool QnThreadedPtzController::createPreset(
    const QnPtzPreset& preset)
{
    return executeCommand(
        Command::createPreset,
        preset,
        &QnAbstractPtzController::createPreset,
        {Type::operational},
        preset);
}

bool QnThreadedPtzController::updatePreset(const QnPtzPreset& preset)
{
    return executeCommand(
        Command::updatePreset,
        preset,
        &QnAbstractPtzController::updatePreset,
        {Type::operational},
        preset);
}

bool QnThreadedPtzController::removePreset(const QString& presetId)
{
    return executeCommand(
        Command::removePreset,
        presetId,
        &QnAbstractPtzController::removePreset,
        {Type::operational},
        presetId);
}

bool QnThreadedPtzController::activatePreset(const QString& presetId, qreal speed)
{
    return executeCommand(
        Command::activatePreset,
        presetId,
        &QnAbstractPtzController::activatePreset,
        {Type::operational},
        presetId,
        speed);
}

bool QnThreadedPtzController::getPresets(QnPtzPresetList* /*presets*/) const
{
    return requestData<QnPtzPresetList>(
        Command::getPresets,
        &QnAbstractPtzController::getPresets,
        {Type::operational});
}

bool QnThreadedPtzController::createTour(const QnPtzTour& tour)
{
    return executeCommand(
        Command::createTour,
        tour,
        &QnAbstractPtzController::createTour,
        {Type::operational},
        tour);
}

bool QnThreadedPtzController::removeTour(const QString& tourId)
{
    return executeCommand(
        Command::removeTour,
        tourId,
        &QnAbstractPtzController::removeTour,
        {Type::operational},
        tourId);
}

bool QnThreadedPtzController::activateTour(const QString& tourId)
{
    return executeCommand(
        Command::activateTour,
        tourId,
        &QnAbstractPtzController::activateTour,
        {Type::operational},
        tourId);
}

bool QnThreadedPtzController::getTours(QnPtzTourList* /*tours*/) const
{
    return requestData<QnPtzTourList>(
        Command::getTours,
        &QnAbstractPtzController::getTours,
        {Type::operational});
}

bool QnThreadedPtzController::getActiveObject(QnPtzObject* /*object*/) const
{
    return requestData<QnPtzObject>(
        Command::getActiveObject,
        &QnAbstractPtzController::getActiveObject,
        {Type::operational});
}

bool QnThreadedPtzController::updateHomeObject(
    const QnPtzObject& homePosition)
{
    return executeCommand(
        Command::updateHomeObject,
        homePosition,
        &QnAbstractPtzController::updateHomeObject,
        {Type::operational},
        homePosition);
}

bool QnThreadedPtzController::getHomeObject(
    QnPtzObject* /*object*/) const
{
    return requestData<QnPtzObject>(
        Command::getHomeObject,
        &QnAbstractPtzController::getHomeObject,
        {Type::operational});
}

bool QnThreadedPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* /*traits*/,
    const Options& options) const
{
    return requestData<QnPtzAuxiliaryTraitList>(
        Command::getAuxiliaryTraits,
        &QnAbstractPtzController::getAuxiliaryTraits,
        options,
        options);
}

bool QnThreadedPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const Options& options)
{
    return executeCommand(
        Command::runAuxiliaryCommand,
        trait,
        &QnAbstractPtzController::runAuxiliaryCommand,
        options,
        trait,
        data,
        options);
}

bool QnThreadedPtzController::getData(
    QnPtzData* /*data*/,
    DataFields query,
    const Options& options) const
{
    return requestData<QnPtzData>(
        Command::getData,
        &QnAbstractPtzController::getData,
        options,
        query,
        options);
}
