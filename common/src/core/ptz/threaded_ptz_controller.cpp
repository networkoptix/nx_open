#include "threaded_ptz_controller.h"

#include <QtCore/QMutex>
#include <QtCore/QThreadPool>

#include "ptz_data.h"


// -------------------------------------------------------------------------- //
// QnPtzCommand
// -------------------------------------------------------------------------- //
class QnAbstractPtzCommand: public QnPtzCommandBase, public QRunnable {
public:
    QnAbstractPtzCommand(const QnPtzControllerPtr &controller, Qn::PtzCommand command, const QVariant &data): 
        m_controller(controller),
        m_command(command),
        m_data(data) 
    {}

    const QnPtzControllerPtr &controller() const {
        return m_controller;
    }
    
    virtual void run() override {
        emit finished(m_command, m_data, runCommand());
    }

    virtual QVariant runCommand() = 0;

private:
    QnPtzControllerPtr m_controller;
    Qn::PtzCommand m_command;
    QVariant m_data;
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
#define QN_RUN_COMMAND(COMMAND, CALL)                                           \
    runCommand(                                                                 \
        COMMAND,                                                                \
        QVariant(),                                                             \
        [=](const QnPtzControllerPtr &controller) -> QVariant {                 \
            return QVariant::fromValue(controller->CALL);                       \
        }                                                                       \
    )

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
    return !baseController->hasCapabilities(Qn::NonBlockingPtzCapability);
}

template<class Functor>
void QnThreadedPtzController::runCommand(Qn::PtzCommand command, const QVariant &data, const Functor &functor) const {
    QnPtzCommand<Functor> *runnable = new QnPtzCommand<Functor>(baseController(), command, data, functor);
    runnable->setAutoDelete(true);
    connect(runnable, &QnAbstractPtzCommand::finished, this, &QnThreadedPtzController::at_ptzCommand_finished, Qt::QueuedConnection);

    d->threadPool->start(runnable);
}

template<class T>
bool QnThreadedPtzController::getField(Qn::PtzDataField field, T QnPtzData::*member, T *target) {
    QMutexLocker locker(&d->mutex);
    if(!(d->data.fields & field))
        return false;

    *target = d->data.*member;
    return true;
}

Qn::PtzCapabilities QnThreadedPtzController::getCapabilities() {
    return baseController()->getCapabilities() | Qn::NonBlockingPtzCapability;
}

bool QnThreadedPtzController::continuousMove(const QVector3D &speed) {
    if(!supports(Qn::ContinuousMovePtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::ContinuousMovePtzCommand, continuousMove(speed));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    
    return true;
}

bool QnThreadedPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    Qn::PtzCommand command = space == Qn::DevicePtzCoordinateSpace ? Qn::AbsoluteDeviceMovePtzCommand : Qn::AbsoluteLogicalMovePtzCommand;
    if(!supports(command))
        return false;

    QN_RUN_COMMAND(command, absoluteMove(space, position, speed));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnThreadedPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!supports(Qn::ViewportMovePtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::ViewportMovePtzCommand, viewportMove(aspectRatio, viewport, speed));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnThreadedPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    Qn::PtzCommand command = space == Qn::DevicePtzCoordinateSpace ? Qn::GetDevicePositionPtzCommand : Qn::GetLogicalPositionPtzCommand;
    if(!supports(command))
        return false;

    if(space == Qn::DevicePtzCoordinateSpace) {
        return getField(Qn::DevicePositionPtzField, &QnPtzData::devicePosition, position);
    } else {
        return getField(Qn::LogicalPositionPtzField, &QnPtzData::logicalPosition, position);
    }
}

bool QnThreadedPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    Qn::PtzCommand command = space == Qn::DevicePtzCoordinateSpace ? Qn::GetDeviceLimitsPtzCommand : Qn::GetLogicalLimitsPtzCommand;
    if(!supports(command))
        return false;

    if(space == Qn::DevicePtzCoordinateSpace) {
        return getField(Qn::DeviceLimitsPtzField, &QnPtzData::deviceLimits, limits);
    } else {
        return getField(Qn::LogicalLimitsPtzField, &QnPtzData::logicalLimits, limits);
    }
}

bool QnThreadedPtzController::getFlip(Qt::Orientations *flip) {
    if(!supports(Qn::GetFlipPtzCommand))
        return false;

    return getField(Qn::FlipPtzField, &QnPtzData::flip, flip);
}

bool QnThreadedPtzController::createPreset(const QnPtzPreset &preset) {
    if(!supports(Qn::CreatePresetPtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::CreatePresetPtzCommand, createPreset(preset));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnThreadedPtzController::updatePreset(const QnPtzPreset &preset) {
    if(!supports(Qn::UpdatePresetPtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::UpdatePresetPtzCommand, updatePreset(preset));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnThreadedPtzController::removePreset(const QString &presetId) {
    if(!supports(Qn::RemovePresetPtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::RemovePresetPtzCommand, removePreset(presetId));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnThreadedPtzController::activatePreset(const QString &presetId, qreal speed) {
    if(!supports(Qn::ActivatePresetPtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::ActivatePresetPtzCommand, activatePreset(presetId, speed));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnThreadedPtzController::getPresets(QnPtzPresetList *presets) {
    if(!supports(Qn::GetPresetsPtzCommand))
        return false;

    return getField(Qn::PresetsPtzField, &QnPtzData::presets, presets);
}

bool QnThreadedPtzController::createTour(const QnPtzTour &tour) {
    if(!supports(Qn::CreateTourPtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::CreateTourPtzCommand, createTour(tour));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::ToursPtzField;

    return true;
}

bool QnThreadedPtzController::removeTour(const QString &tourId) {
    if(!supports(Qn::RemoveTourPtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::RemoveTourPtzCommand, removeTour(tourId));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::ToursPtzField;

    return true;
}

bool QnThreadedPtzController::activateTour(const QString &tourId) {
    if(!supports(Qn::ActivateTourPtzCommand))
        return false;

    QN_RUN_COMMAND(Qn::ActivateTourPtzCommand, activateTour(tourId));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnThreadedPtzController::getTours(QnPtzTourList *tours) {
    if(!supports(Qn::GetToursPtzCommand))
        return false;

    return getField(Qn::ToursPtzField, &QnPtzData::tours, tours);
}

void QnThreadedPtzController::synchronize(Qn::PtzDataFields fields) {
    /* Don't call the base implementation as it calls the target's synchronize directly. */

    runCommand(
        Qn::SynchronizePtzCommand, 
        QVariant::fromValue<Qn::PtzDataFields>(fields), 
        [=](const QnPtzControllerPtr &controller) -> QVariant {
            controller->synchronize(fields);

            QnPtzData data;
            controller->getData(fields, &data);
            return QVariant::fromValue<QnPtzData>(data);
        }
    );
}

void QnThreadedPtzController::at_ptzCommand_finished(Qn::PtzCommand command, const QVariant &data, const QVariant &result) {
    // TODO: merge data
}
