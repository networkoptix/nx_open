#include "threaded_ptz_controller.h"

#include <QtCore/QThreadPool>

#include <common/common_meta_types.h>
#include <core/ptz/ptz_controller_pool.h>
#include <nx/utils/thread/mutex.h>

using namespace nx::core;

class QnAbstractPtzCommand: public QnPtzCommandBase, public QRunnable
{
public:
    QnAbstractPtzCommand(const QnPtzControllerPtr &controller, Qn::PtzCommand command):
        m_controller(controller),
        m_command(command)
    {
    }

    const QnPtzControllerPtr& controller() const
    {
        return m_controller;
    }

    Qn::PtzCommand command() const
    {
        return m_command;
    }

    virtual void run() override
    {
        emit finished(m_command, runCommand());
    }

    virtual QVariant runCommand() = 0;

private:
    QnPtzControllerPtr m_controller;
    Qn::PtzCommand m_command;
};

template<class Functor>
class QnPtzCommand: public QnAbstractPtzCommand
{
public:
    QnPtzCommand(
        const QnPtzControllerPtr& controller,
        Qn::PtzCommand command,
        const Functor& functor)
        :
        QnAbstractPtzCommand(controller, command),
        m_functor(functor)
    {
    }

    virtual QVariant runCommand() override
    {
        return m_functor(controller());
    }

private:
    Functor m_functor;
};


// TODO: #Elric get rid of this macro hell
#define RUN_COMMAND(COMMAND, RESULT_TYPE, RETURN_VALUE, FUNCTION, OPTIONS, ... /* PARAMS */) \
    {                                                                               \
        const auto nonConstThis = const_cast<QnThreadedPtzController*>(this);       \
        const Qn::PtzCommand command = COMMAND;                                     \
        const nx::core::ptz::Options internalOptions = OPTIONS;                     \
        if (!nonConstThis->supports(command, internalOptions))                      \
            return false;                                                           \
                                                                                    \
        runCommand(command,                                                         \
            [=](const QnPtzControllerPtr &controller) -> QVariant                   \
            {                                                                       \
                RESULT_TYPE result;                                                 \
                Q_UNUSED(result);                                                   \
                if (controller->FUNCTION(__VA_ARGS__))                              \
                    return QVariant::fromValue(RETURN_VALUE);                       \
                else                                                                \
                    return QVariant();                                              \
            }                                                                       \
        );                                                                          \
                                                                                    \
        return true;                                                                \
    }

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

template<class Functor>
void QnThreadedPtzController::runCommand(Qn::PtzCommand command, const Functor& functor) const
{
    QnPtzCommand<Functor>* runnable =
        new QnPtzCommand<Functor>(baseController(), command, functor);
    runnable->setAutoDelete(true);
    connect(runnable, &QnAbstractPtzCommand::finished,
        this, &QnAbstractPtzController::finished, Qt::QueuedConnection);

    m_threadPool->start(runnable);
}

Ptz::Capabilities QnThreadedPtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    return extends(capabilities) ? (capabilities | Ptz::AsynchronousPtzCapability) : capabilities;
}

bool QnThreadedPtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(
        Qn::ContinuousMovePtzCommand,
        void*,
        speed,
        continuousMove,
        options,
        speed,
        options);
}

bool QnThreadedPtzController::continuousFocus(
    qreal speed,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(
        Qn::ContinuousFocusPtzCommand, void* , speed, continuousFocus, options, speed, options);
}

bool QnThreadedPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space), void*,
        position, absoluteMove, options, space, position, speed, options);
}

bool QnThreadedPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(Qn::ViewportMovePtzCommand, void*,
                viewport, viewportMove, options, aspectRatio, viewport, speed, options);
}

bool QnThreadedPtzController::relativeMove(
    const ptz::Vector& direction,
    const ptz::Options& options)
{
    RUN_COMMAND(
        Qn::RelativeMovePtzCommand,
        void*,
        direction,
        relativeMove,
        options,
        direction,
        options);
}

bool QnThreadedPtzController::relativeFocus(qreal direction, const ptz::Options& options)
{
    RUN_COMMAND(
        Qn::RelativeFocusPtzCommand,
        void*,
        direction,
        relativeFocus,
        options,
        direction,
        options);
}

bool QnThreadedPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* /*position*/,
    const nx::core::ptz::Options& options) const
{
    RUN_COMMAND(spaceCommand(Qn::GetDevicePositionPtzCommand, space), nx::core::ptz::Vector,
        result, getPosition, options, space, &result, options);
}

bool QnThreadedPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* /*limits*/,
    const nx::core::ptz::Options& options) const
{
    RUN_COMMAND(spaceCommand(Qn::GetDeviceLimitsPtzCommand, space), QnPtzLimits,
        result, getLimits, options, space, &result, options);
}

bool QnThreadedPtzController::getFlip(
    Qt::Orientations* /*flip*/,
    const nx::core::ptz::Options& options) const
{
    RUN_COMMAND(
        Qn::GetFlipPtzCommand, Qt::Orientations, result, getFlip, options, &result, options);
}

bool QnThreadedPtzController::createPreset(
    const QnPtzPreset& preset)
{
    RUN_COMMAND(
        Qn::CreatePresetPtzCommand,
        void*,
        preset,
        createPreset,
        {nx::core::ptz::Type::operational},
        preset);
}

bool QnThreadedPtzController::updatePreset(const QnPtzPreset& preset)
{
    RUN_COMMAND(
        Qn::UpdatePresetPtzCommand,
        void*,
        preset,
        updatePreset,
        {nx::core::ptz::Type::operational},
        preset);
}

bool QnThreadedPtzController::removePreset(const QString& presetId)
{
    RUN_COMMAND(
        Qn::RemovePresetPtzCommand,
        void*,
        presetId,
        removePreset,
        {nx::core::ptz::Type::operational},
        presetId);
}

bool QnThreadedPtzController::activatePreset(const QString& presetId, qreal speed)
{
    RUN_COMMAND(
        Qn::ActivatePresetPtzCommand,
        void*,
        presetId,
        activatePreset,
        {nx::core::ptz::Type::operational},
        presetId,
        speed);
}

bool QnThreadedPtzController::getPresets(QnPtzPresetList* /*presets*/) const
{
    RUN_COMMAND(
        Qn::GetPresetsPtzCommand,
        QnPtzPresetList,
        result,
        getPresets,
        {nx::core::ptz::Type::operational},
        &result);
}

bool QnThreadedPtzController::createTour(const QnPtzTour& tour)
{
    RUN_COMMAND(
        Qn::CreateTourPtzCommand,
        void*,
        tour,
        createTour,
        {nx::core::ptz::Type::operational},
        tour);
}

bool QnThreadedPtzController::removeTour(const QString& tourId)
{
    RUN_COMMAND(
        Qn::RemoveTourPtzCommand,
        void*,
        tourId,
        removeTour,
        {nx::core::ptz::Type::operational},
        tourId);
}

bool QnThreadedPtzController::activateTour(const QString& tourId)
{
    RUN_COMMAND(
        Qn::ActivateTourPtzCommand,
        void*,
        tourId,
        activateTour,
        {nx::core::ptz::Type::operational},
        tourId);
}

bool QnThreadedPtzController::getTours(QnPtzTourList* /*tours*/) const
{
    RUN_COMMAND(
        Qn::GetToursPtzCommand,
        QnPtzTourList,
        result,
        getTours,
        {nx::core::ptz::Type::operational},
        &result);
}

bool QnThreadedPtzController::getActiveObject(QnPtzObject* /*object*/) const
{
    RUN_COMMAND(
        Qn::GetActiveObjectPtzCommand,
        QnPtzObject,
        result,
        getActiveObject,
        {nx::core::ptz::Type::operational},
        &result);
}

bool QnThreadedPtzController::updateHomeObject(
    const QnPtzObject& homePosition)
{
    RUN_COMMAND(
        Qn::UpdateHomeObjectPtzCommand,
        void*,
        homePosition,
        updateHomeObject,
        {nx::core::ptz::Type::operational},
        homePosition);
}

bool QnThreadedPtzController::getHomeObject(
    QnPtzObject* /*object*/) const
{
    RUN_COMMAND(
        Qn::GetHomeObjectPtzCommand,
        QnPtzObject,
        result,
        getHomeObject,
        {nx::core::ptz::Type::operational},
        &result);
}

bool QnThreadedPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* /*traits*/,
    const nx::core::ptz::Options& options) const
{
    RUN_COMMAND(Qn::GetAuxiliaryTraitsPtzCommand, QnPtzAuxiliaryTraitList,
        result, getAuxiliaryTraits, options, &result, options);
}

bool QnThreadedPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const nx::core::ptz::Options& options)
{
    RUN_COMMAND(
        Qn::RunAuxiliaryCommandPtzCommand,
        void*,
        trait,
        runAuxiliaryCommand,
        options,
        trait,
        data,
        options);
}

bool QnThreadedPtzController::getData(
    Qn::PtzDataFields query,
    QnPtzData* /*data*/,
    const nx::core::ptz::Options& options) const
{
    RUN_COMMAND(
        Qn::GetDataPtzCommand,
        QnPtzData,
        result,
        getData,
        options,
        query,
        &result,
        options);
}


