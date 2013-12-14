#include "threaded_ptz_controller.h"

#include <QtCore/QMutex>
#include <QtCore/QThreadPool>

#include "ptz_data.h"


// -------------------------------------------------------------------------- //
// QnPtzCommand
// -------------------------------------------------------------------------- //
class QnAbstractPtzCommand: public QnPtzCommandBase, public QRunnable {
public:
    QnAbstractPtzCommand(const QnPtzControllerPtr &controller, Qn::PtzCommand command): 
        m_controller(controller),
        m_command(command)
    {}

    const QnPtzControllerPtr &controller() const {
        return m_controller;
    }
    
    Qn::PtzCommand command() const {
        return m_command;
    }

    virtual void run() override {
        emit finished(m_command, runCommand());
    }

    virtual QVariant runCommand() = 0;

private:
    QnPtzControllerPtr m_controller;
    Qn::PtzCommand m_command;
};

template<class Functor>
class QnPtzCommand: public QnAbstractPtzCommand {
public:
    QnPtzCommand(const QnPtzControllerPtr &controller, Qn::PtzCommand command, const QVariant &data, const Functor &functor):
        QnAbstractPtzCommand(controller, command, data),
        m_functor(functor)
    {}

    virtual QVariant runCommand() override {
        return m_functor(controller());
    }

private:
    Functor m_functor;
};


// -------------------------------------------------------------------------- //
// QnPtzCommandThreadPool
// -------------------------------------------------------------------------- //
class QnPtzCommandThreadPool: public QThreadPool {
public:
    QnPtzCommandThreadPool() {
        setMaxThreadCount(1024); /* Should be enough for our needs =). */
    }
};

Q_GLOBAL_STATIC(QnPtzCommandThreadPool, qn_ptzCommandThreadPool_instance)


// -------------------------------------------------------------------------- //
// QnThreadedPtzControllerPrivate
// -------------------------------------------------------------------------- //
class QnThreadedPtzControllerPrivate {
public:
    QnThreadedPtzControllerPrivate(): threadPool(NULL) {}

    void init() {
        threadPool = qn_ptzCommandThreadPool_instance();
    }

    QnThreadedPtzController *q;

    QThreadPool *threadPool;
    QMutex mutex;
    QnPtzData data;
};


// -------------------------------------------------------------------------- //
// QnThreadedPtzController
// -------------------------------------------------------------------------- //
#define RUN_COMMAND(COMMAND, RESULT_TYPE, RETURN_VALUE, FUNCTION, ... /* PARAMS */) \
    {                                                                           \
        Qn::PtzCommand command = COMMAND;                                       \
        if(!supports(command))                                                  \
            return false;                                                       \
                                                                                \
        runCommand(                                                             \
            command,                                                            \
            [=](const QnPtzControllerPtr &controller) -> QVariant {             \
                RESULT_TYPE result;                                             \
                Q_UNUSED(result);                                               \
                bool status = controller->FUNCTION(__VA_ARGS__);                \
                if(!status) {                                                   \
                    return QVariant();                                          \
                } else {                                                        \
                    return QVariant::fromValue(RETURN_VALUE);                   \
                }                                                               \
            }                                                                   \
        );                                                                      \
                                                                                \
        return true;                                                            \
    }

#define RUN_GET_COMMAND(COMMAND, RESULT_TYPE, FUNCTION, ... /* PARAMS */) RUN_COMMAND(COMMAND, RESULT_TYPE, result, FUNCTION, ##__VA_ARGS__, &result)
#define RUN_ACT_COMMAND(COMMAND, FUNCTION, ... /* PARAMS */) RUN_COMMAND(COMMAND, bool, status, FUNCTION, ##__VA_ARGS__)

QnThreadedPtzController::QnThreadedPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    d(new QnThreadedPtzControllerPrivate)
{
    d->q = this;
    d->init();
}

QnThreadedPtzController::~QnThreadedPtzController() {
    return;
}

bool QnThreadedPtzController::extends(const QnPtzControllerPtr &baseController) {
    return !baseController->hasCapabilities(Qn::AsynchronousPtzCapability);
}

template<class Functor>
void QnThreadedPtzController::runCommand(Qn::PtzCommand command, const Functor &functor) const {
    QnPtzCommand<Functor> *runnable = new QnPtzCommand<Functor>(baseController(), command, functor);
    runnable->setAutoDelete(true);
    connect(runnable, &QnAbstractPtzCommand::finished, this, &QnAbstractPtzController::finished, Qt::QueuedConnection);

    d->threadPool->start(runnable);
}

Qn::PtzCapabilities QnThreadedPtzController::getCapabilities() {
    return baseController()->getCapabilities() | Qn::AsynchronousPtzCapability;
}

bool QnThreadedPtzController::continuousMove(const QVector3D &speed) {
    RUN_ACT_COMMAND(Qn::ContinuousMovePtzCommand, continuousMove, speed);
}

bool QnThreadedPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    RUN_ACT_COMMAND(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space), absoluteMove, space, position, speed);
}

bool QnThreadedPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    RUN_ACT_COMMAND(Qn::ViewportMovePtzCommand, viewportMove, aspectRatio, viewport, speed);
}

bool QnThreadedPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *) {
    RUN_GET_COMMAND(spaceCommand(Qn::GetDevicePositionPtzCommand, space), QVector3D, getPosition, space);
}

bool QnThreadedPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *) {
    RUN_GET_COMMAND(spaceCommand(Qn::GetDeviceLimitsPtzCommand, space), QnPtzLimits, getLimits, space);
}

bool QnThreadedPtzController::getFlip(Qt::Orientations *) {
    RUN_GET_COMMAND(Qn::GetFlipPtzCommand, Qt::Orientations, getFlip);
}

bool QnThreadedPtzController::createPreset(const QnPtzPreset &preset) {
    RUN_ACT_COMMAND(Qn::CreatePresetPtzCommand, createPreset, preset);
}

bool QnThreadedPtzController::updatePreset(const QnPtzPreset &preset) {
    RUN_ACT_COMMAND(Qn::UpdatePresetPtzCommand, updatePreset, preset);
}

bool QnThreadedPtzController::removePreset(const QString &presetId) {
    RUN_ACT_COMMAND(Qn::RemovePresetPtzCommand, removePreset, presetId);
}

bool QnThreadedPtzController::activatePreset(const QString &presetId, qreal speed) {
    RUN_ACT_COMMAND(Qn::ActivatePresetPtzCommand, activatePreset, presetId, speed);
}

bool QnThreadedPtzController::getPresets(QnPtzPresetList *presets) {
    RUN_GET_COMMAND(Qn::GetPresetsPtzCommand, QnPtzPresetList, getPresets);
}

bool QnThreadedPtzController::createTour(const QnPtzTour &tour) {
    RUN_ACT_COMMAND(Qn::CreateTourPtzCommand, createTour, tour);
}

bool QnThreadedPtzController::removeTour(const QString &tourId) {
    RUN_ACT_COMMAND(Qn::RemoveTourPtzCommand, removeTour, tourId);
}

bool QnThreadedPtzController::activateTour(const QString &tourId) {
    RUN_ACT_COMMAND(Qn::ActivateTourPtzCommand, activateTour, tourId);
}

bool QnThreadedPtzController::getTours(QnPtzTourList *tours) {
    RUN_GET_COMMAND(Qn::GetToursPtzCommand, QnPtzTourList, getTours);
}

bool QnThreadedPtzController::synchronize(Qn::PtzDataFields fields) {
    RUN_ACT_COMMAND(Qn::SynchronizePtzCommand, synchronize, fields);
}

