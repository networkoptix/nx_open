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
    QnAbstractPtzCommand(const QnPtzControllerPtr &controller, Qn::PtzDataFields fields): 
        m_controller(controller),
        m_fields(fields) 
    {}

    const QnPtzControllerPtr &controller() const {
        return m_controller;
    }
    
    Qn::PtzDataFields fields() const {
        return m_fields;
    }

    virtual void run() override {
        QVariant result;
        bool status = runCommand(&result);

        emit finished(m_fields, status, result);
    }

    virtual bool runCommand(QVariant *result) = 0;

private:
    QnPtzControllerPtr m_controller;
    Qn::PtzDataFields m_fields;
};

template<class Functor>
class QnPtzCommand: public QnAbstractPtzCommand {
public:
    QnPtzCommand(const QnPtzControllerPtr &controller, Qn::PtzDataFields fields, const Functor &functor):
        QnAbstractPtzCommand(controller, fields),
        m_functor(functor)
    {}

    virtual bool runCommand(QVariant *result) override {
        return m_functor(controller(), result);
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
void QnNonBlockingPtzController::runCommand(Qn::PtzDataFields fields, const Functor &functor) const {
    QnPtzCommand<Functor> *command = new QnPtzCommand<Functor>(baseController(), fields, functor);
    command->setAutoDelete(true);
    connect(command, &QnAbstractPtzCommand::finished, this, &QnNonBlockingPtzController::at_ptzCommand_finished, Qt::QueuedConnection);

    d->threadPool->start(command);
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

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->continuousMove(speed);
    });

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    
    return true;
}

bool QnNonBlockingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::AbsolutePtzCapabilities) || !hasSpaceCapabilities(capabilities, space))
        return false;

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->absoluteMove(space, position, speed);
    });

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);

    return true;
}

bool QnNonBlockingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ViewportPtzCapability))
        return false;

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->viewportMove(aspectRatio, viewport, speed);
    });

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

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->createPreset(preset);
    });

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnNonBlockingPtzController::updatePreset(const QnPtzPreset &preset) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->updatePreset(preset);
    });

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnNonBlockingPtzController::removePreset(const QString &presetId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->removePreset(presetId);
    });

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::PresetsPtzField;

    return true;
}

bool QnNonBlockingPtzController::activatePreset(const QString &presetId, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->activatePreset(presetId, speed);
    });

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

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->createTour(tour);
    });

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::ToursPtzCapability;

    return true;
}

bool QnNonBlockingPtzController::removeTour(const QString &tourId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->removeTour(tourId);
    });

    QMutexLocker locker(&d->mutex);
    d->data.fields &= ~Qn::ToursPtzCapability;

    return true;
}

bool QnNonBlockingPtzController::activateTour(const QString &tourId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->activateTour(tourId);
    });

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

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *value) -> bool {
        controller->synchronize(fields);

        QnPtzData data;
        controller->getData(fields, &data);
        return true;
        // TODO: pass data out
    });
}

void QnNonBlockingPtzController::at_ptzCommand_finished(Qn::PtzDataFields fields, bool status, const QVariant &result) {
    // TODO: merge data
}
