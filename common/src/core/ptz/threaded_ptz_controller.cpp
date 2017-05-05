#include "threaded_ptz_controller.h"

#include <QtCore/QThreadPool>


#include <common/common_meta_types.h>
#include <core/ptz/ptz_controller_pool.h>
#include <nx/utils/thread/mutex.h>

class QnAbstractPtzCommand: public QnPtzCommandBase, public QRunnable
{
public:
    QnAbstractPtzCommand(const QnPtzControllerPtr& controller, Qn::PtzCommand command):
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
#define RUN_COMMAND(COMMAND, RESULT_TYPE, RETURN_VALUE, FUNCTION, ... /* PARAMS */) \
    {                                                                               \
        const auto nonConstThis = const_cast<QnThreadedPtzController*>(this);       \
        const Qn::PtzCommand command = COMMAND;                                     \
        if (!nonConstThis->supports(command))                                       \
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

QnThreadedPtzController::QnThreadedPtzController(const QnPtzControllerPtr& baseController):
    base_type(baseController),
    m_threadPool(qnPtzPool->commandThreadPool())
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

Ptz::Capabilities QnThreadedPtzController::getCapabilities() const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Ptz::AsynchronousPtzCapability) : capabilities;
}

bool QnThreadedPtzController::continuousMove(const QVector3D& speed)
{
    RUN_COMMAND(Qn::ContinuousMovePtzCommand, void* , speed, continuousMove, speed);
}

bool QnThreadedPtzController::continuousFocus(qreal speed)
{
    RUN_COMMAND(Qn::ContinuousFocusPtzCommand, void* , speed, continuousFocus, speed);
}

bool QnThreadedPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const QVector3D& position,
    qreal speed)
{
    RUN_COMMAND(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space), void*,
        position, absoluteMove, space, position, speed);
}

bool QnThreadedPtzController::viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed)
{
    RUN_COMMAND(Qn::ViewportMovePtzCommand, void*,
        viewport, viewportMove, aspectRatio, viewport, speed);
}

bool QnThreadedPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    QVector3D* /*position*/) const
{
    RUN_COMMAND(spaceCommand(Qn::GetDevicePositionPtzCommand, space), QVector3D,
        result, getPosition, space, &result);
}

bool QnThreadedPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* /*limits*/) const
{
    RUN_COMMAND(spaceCommand(Qn::GetDeviceLimitsPtzCommand, space), QnPtzLimits,
        result, getLimits, space, &result);
}

bool QnThreadedPtzController::getFlip(Qt::Orientations* /*flip*/) const
{
    RUN_COMMAND(Qn::GetFlipPtzCommand, Qt::Orientations, result, getFlip, &result);
}

bool QnThreadedPtzController::createPreset(const QnPtzPreset& preset)
{
    RUN_COMMAND(Qn::CreatePresetPtzCommand, void*, preset, createPreset, preset);
}

bool QnThreadedPtzController::updatePreset(const QnPtzPreset& preset)
{
    RUN_COMMAND(Qn::UpdatePresetPtzCommand, void*, preset, updatePreset, preset);
}

bool QnThreadedPtzController::removePreset(const QString& presetId)
{
    RUN_COMMAND(Qn::RemovePresetPtzCommand, void*, presetId, removePreset, presetId);
}

bool QnThreadedPtzController::activatePreset(const QString& presetId, qreal speed)
{
    RUN_COMMAND(Qn::ActivatePresetPtzCommand, void*, presetId, activatePreset, presetId, speed);
}

bool QnThreadedPtzController::getPresets(QnPtzPresetList* /*presets*/) const
{
    RUN_COMMAND(Qn::GetPresetsPtzCommand, QnPtzPresetList, result, getPresets, &result);
}

bool QnThreadedPtzController::createTour(const QnPtzTour& tour)
{
    RUN_COMMAND(Qn::CreateTourPtzCommand, void*, tour, createTour, tour);
}

bool QnThreadedPtzController::removeTour(const QString& tourId)
{
    RUN_COMMAND(Qn::RemoveTourPtzCommand, void*, tourId, removeTour, tourId);
}

bool QnThreadedPtzController::activateTour(const QString& tourId)
{
    RUN_COMMAND(Qn::ActivateTourPtzCommand, void*, tourId, activateTour, tourId);
}

bool QnThreadedPtzController::getTours(QnPtzTourList* /*tours*/) const
{
    RUN_COMMAND(Qn::GetToursPtzCommand, QnPtzTourList, result, getTours, &result);
}

bool QnThreadedPtzController::getActiveObject(QnPtzObject* /*object*/) const
{
    RUN_COMMAND(Qn::GetActiveObjectPtzCommand, QnPtzObject, result, getActiveObject, &result);
}

bool QnThreadedPtzController::updateHomeObject(const QnPtzObject& homePosition)
{
    RUN_COMMAND(Qn::UpdateHomeObjectPtzCommand, void*,
        homePosition, updateHomeObject, homePosition);
}

bool QnThreadedPtzController::getHomeObject(QnPtzObject* /*object*/) const
{
    RUN_COMMAND(Qn::GetHomeObjectPtzCommand, QnPtzObject, result, getHomeObject, &result);
}

bool QnThreadedPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList* /*traits*/) const
{
    RUN_COMMAND(Qn::GetAuxilaryTraitsPtzCommand, QnPtzAuxilaryTraitList,
        result, getAuxilaryTraits, &result);
}

bool QnThreadedPtzController::runAuxilaryCommand(
    const QnPtzAuxilaryTrait& trait,
    const QString& data)
{
    RUN_COMMAND(Qn::RunAuxilaryCommandPtzCommand, void*, trait, runAuxilaryCommand, trait, data);
}

bool QnThreadedPtzController::getData(Qn::PtzDataFields query, QnPtzData* /*data*/) const
{
    RUN_COMMAND(Qn::GetDataPtzCommand, QnPtzData, result, getData, query, &result);
}


