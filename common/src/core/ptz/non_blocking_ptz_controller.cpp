#include "non_blocking_ptz_controller.h"

#include <QtCore/QMutex>
#include <QtCore/QThreadPool>

#include "ptz_data.h"

namespace {
    bool hasSpaceCapabilities(Qn::PtzCapabilities capabilities, Qn::PtzCoordinateSpace space) {
        switch(space) {
        case Qn::DevicePtzCoordinateSpace:     return capabilities & Qn::DevicePositioningPtzCapability;
        case Qn::LogicalPtzCoordinateSpace:    return capabilities & Qn::LogicalPositioningPtzCapability;
        default:                            return false; /* We should never get here. */
        }
    }

} // anonymous namespace


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
// QnNonBlockingPtzControllerPrivate
// -------------------------------------------------------------------------- //
class QnNonBlockingPtzControllerPrivate {
public:
    QnNonBlockingPtzControllerPrivate(): threadPool(NULL) {}

    void init() {
        threadPool = qn_ptzCommandThreadPool_instance();
    }

    QnNonBlockingPtzController *q;

    QThreadPool *threadPool;
    QMutex mutex;
    QnPtzData data;
};


// -------------------------------------------------------------------------- //
// QnNonBlockingPtzController
// -------------------------------------------------------------------------- //
#define QN_RUN_COMMAND(COMMAND, CALL)                                           \
    runCommand(                                                                 \
        COMMAND,                                                                \
        QVariant(),                                                             \
        [=](const QnPtzControllerPtr &controller) -> QVariant {                 \
            return QVariant::fromValue(controller->CALL);                       \
        }                                                                       \
    )

QnNonBlockingPtzController::QnNonBlockingPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    d(new QnNonBlockingPtzControllerPrivate)
{
    d->q = this;
    d->init();
}

QnNonBlockingPtzController::~QnNonBlockingPtzController() {
    return;
}

bool QnNonBlockingPtzController::extends(const QnPtzControllerPtr &baseController) {
    return !baseController->hasCapabilities(Qn::NonBlockingPtzCapability);
}

template<class Functor>
void QnNonBlockingPtzController::runCommand(Qn::PtzCommand command, const QVariant &data, const Functor &functor) const {
    QnPtzCommand<Functor> *runnable = new QnPtzCommand<Functor>(baseController(), command, data, functor);
    runnable->setAutoDelete(true);
    connect(runnable, &QnAbstractPtzCommand::finished, this, &QnNonBlockingPtzController::at_ptzCommand_finished, Qt::QueuedConnection);

    d->threadPool->start(runnable);
}

template<class T>
bool QnNonBlockingPtzController::getField(Qn::PtzDataField field, T QnPtzData::*member, T *target) {
    QMutexLocker locker(&d->mutex);
    if(!(d->data.fields & field))
        return false;

    *target = d->data.*member;
    return true;
}

Qn::PtzCapabilities QnNonBlockingPtzController::getCapabilities() {
    return baseController()->getCapabilities() | Qn::NonBlockingPtzCapability;
}

bool QnNonBlockingPtzController::continuousMove(const QVector3D &speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ContinuousPtzCapabilities))
        return false;

    QN_RUN_COMMAND(Qn::ContinousMovePtzCommand, continuousMove(speed));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    
    return true;
}

bool QnNonBlockingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::AbsolutePtzCapabilities) || !hasSpaceCapabilities(capabilities, space))
        return false;

    QN_RUN_COMMAND(Qn::AbsoluteMovePtzCommand, absoluteMove(space, position, speed));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnNonBlockingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ViewportPtzCapability))
        return false;

    QN_RUN_COMMAND(Qn::ViewportMovePtzCommand, viewportMove(aspectRatio, viewport, speed));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnNonBlockingPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::AbsolutePtzCapabilities) || !hasSpaceCapabilities(capabilities, space))
        return false;

    if(space == Qn::DevicePtzCoordinateSpace) {
        return getField(Qn::DevicePositionPtzField, &QnPtzData::devicePosition, position);
    } else {
        return getField(Qn::LogicalPositionPtzField, &QnPtzData::logicalPosition, position);
    }
}

bool QnNonBlockingPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::LimitsPtzCapability) || !hasSpaceCapabilities(capabilities, space))
        return false;

    if(space == Qn::DevicePtzCoordinateSpace) {
        return getField(Qn::DeviceLimitsPtzField, &QnPtzData::deviceLimits, limits);
    } else {
        return getField(Qn::LogicalLimitsPtzField, &QnPtzData::logicalLimits, limits);
    }
}

bool QnNonBlockingPtzController::getFlip(Qt::Orientations *flip) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::FlipPtzCapability))
        return false;

    return getField(Qn::FlipPtzField, &QnPtzData::flip, flip);
}

bool QnNonBlockingPtzController::createPreset(const QnPtzPreset &preset) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    QN_RUN_COMMAND(Qn::CreatePresetPtzCommand, createPreset(preset));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnNonBlockingPtzController::updatePreset(const QnPtzPreset &preset) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    QN_RUN_COMMAND(Qn::UpdatePresetPtzCommand, updatePreset(preset));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnNonBlockingPtzController::removePreset(const QString &presetId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    QN_RUN_COMMAND(Qn::RemovePresetPtzCommand, removePreset(presetId));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnNonBlockingPtzController::activatePreset(const QString &presetId, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    QN_RUN_COMMAND(Qn::ActivatePresetPtzCommand, activatePreset(presetId, speed));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnNonBlockingPtzController::getPresets(QnPtzPresetList *presets) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    return getField(Qn::PresetsPtzField, &QnPtzData::presets, presets);
}

bool QnNonBlockingPtzController::createTour(const QnPtzTour &tour) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    QN_RUN_COMMAND(Qn::CreateTourPtzCommand, createTour(tour));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::ToursPtzCapability;

    return true;
}

bool QnNonBlockingPtzController::removeTour(const QString &tourId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    QN_RUN_COMMAND(Qn::RemoveTourPtzCommand, removeTour(tourId));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::ToursPtzCapability;

    return true;
}

bool QnNonBlockingPtzController::activateTour(const QString &tourId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    QN_RUN_COMMAND(Qn::ActivateTourPtzCommand, activateTour(tourId));

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnNonBlockingPtzController::getTours(QnPtzTourList *tours) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    return getField(Qn::ToursPtzField, &QnPtzData::tours, tours);
}

void QnNonBlockingPtzController::synchronize(Qn::PtzDataFields fields) {
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

void QnNonBlockingPtzController::at_ptzCommand_finished(Qn::PtzCommand command, const QVariant &data, const QVariant &result) {
    // TODO: merge data
}
