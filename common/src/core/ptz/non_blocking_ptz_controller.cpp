#include "non_blocking_ptz_controller.h"

#include <QtCore/QMutex>
#include <QtCore/QThreadPool>

#include "ptz_data.h"


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
}

QnNonBlockingPtzController::~QnNonBlockingPtzController() {
    return;
}

bool QnNonBlockingPtzController::extends(const QnPtzControllerPtr &baseController) {
    return !baseController->hasCapabilities(Qn::NonBlockingPtzCapability);
}

template<class Functor>
void QnNonBlockingPtzController::runCommand(Qn::PtzDataFields fields, const Functor &functor) {
    QnPtzCommand<Functor> *command = new QnPtzCommand<Functor>(baseController(), fields, functor);
    command->setAutoDelete(true);
    connect(command, &QnAbstractPtzCommand::finished, this, &QnNonBlockingPtzController::at_ptzCommand_finished, Qt::QueuedConnection);

    d->threadPool->start(command);
}

bool QnNonBlockingPtzController::hasSpaceCapabilities(Qn::PtzCapabilities capabilities, Qn::PtzCoordinateSpace space) const {
    switch(space) {
    case Qn::DeviceCoordinateSpace:     return capabilities & Qn::DevicePositioningPtzCapability;
    case Qn::LogicalCoordinateSpace:    return capabilities & Qn::LogicalPositioningPtzCapability;
    default:                            return false; /* We should never get here. */
    }
}

Qn::PtzCapabilities QnNonBlockingPtzController::getCapabilities() {
    return baseController()->getCapabilities() | Qn::NonBlockingPtzCapability;
}

bool QnNonBlockingPtzController::continuousMove(const QVector3D &speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ContinuousPtzCapabilities))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->continuousMove(speed);
    });
    return true;
}

bool QnNonBlockingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::AbsolutePtzCapabilities) || !hasSpaceCapabilities(capabilities, space))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->absoluteMove(space, position, speed);
    });
    return true;
}

bool QnNonBlockingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ViewportPtzCapability))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->viewportMove(aspectRatio, viewport, speed);
    });
    return true;
}

bool QnNonBlockingPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::AbsolutePtzCapabilities) || !hasSpaceCapabilities(capabilities, space))
        return false;

    Qn::PtzDataField field = space == Qn::DeviceCoordinateSpace ? Qn::PtzDevicePositionField : Qn::PtzLogicalPositionField;

    QMutexLocker locker(&d->mutex);
    if(!(d->data.fields & field)) {
        locker.unlock();
        synchronize(field);
        return false;
    }

    if(space == Qn::DeviceCoordinateSpace) {
        *position = d->data.devicePosition;
    } else {
        *position = d->data.logicalPosition;
    }
    return true;
}

bool QnNonBlockingPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::LimitsPtzCapability) || !hasSpaceCapabilities(capabilities, space))
        return false;
    
    Qn::PtzDataField field = space == Qn::DeviceCoordinateSpace ? Qn::PtzDeviceLimitsField : Qn::PtzLogicalLimitsField;

    QMutexLocker locker(&d->mutex);
    if(!(d->data.fields & field)) {
        locker.unlock();
        synchronize(field);
        return false;
    }

    if(space == Qn::DeviceCoordinateSpace) {
        *limits = d->data.deviceLimits;
    } else {
        *limits = d->data.logicalLimits;
    }
    return true;
}

bool QnNonBlockingPtzController::getFlip(Qt::Orientations *flip) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::FlipPtzCapability))
        return false;

    Qn::PtzDataField field = Qn::PtzFlipField;

    QMutexLocker locker(&d->mutex);
    if(!(d->data.fields & field)) {
        locker.unlock();
        synchronize(field);
        return false;
    }

    *flip = d->data.flip;
    return true;
}

bool QnNonBlockingPtzController::createPreset(const QnPtzPreset &preset) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->createPreset(preset);
    });
    return true;
}

bool QnNonBlockingPtzController::updatePreset(const QnPtzPreset &preset) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->updatePreset(preset);
    });
    return true;
}

bool QnNonBlockingPtzController::removePreset(const QString &presetId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->removePreset(presetId);
    });
    return true;
}

bool QnNonBlockingPtzController::activatePreset(const QString &presetId, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->activatePreset(presetId, speed);
    });
    return true;
}

bool QnNonBlockingPtzController::getPresets(QnPtzPresetList *presets) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::PresetsPtzCapability))
        return false;

    Qn::PtzDataField field = Qn::PtzPresetsField;

    QMutexLocker locker(&d->mutex);
    if(!(d->data.fields & field)) {
        locker.unlock();
        synchronize(field);
        return false;
    }

    *presets = d->data.presets;
    return true;
}

bool QnNonBlockingPtzController::createTour(const QnPtzTour &tour) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->createTour(tour);
    });
    return true;
}

bool QnNonBlockingPtzController::removeTour(const QString &tourId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->removeTour(tourId);
    });
    return true;
}

bool QnNonBlockingPtzController::activateTour(const QString &tourId) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    // TODO: invalidate

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->activateTour(tourId);
    });
    return true;
}

bool QnNonBlockingPtzController::getTours(QnPtzTourList *tours) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ToursPtzCapability))
        return false;

    Qn::PtzDataField field = Qn::PtzToursField;

    QMutexLocker locker(&d->mutex);
    if(!(d->data.fields & field)) {
        locker.unlock();
        synchronize(field);
        return false;
    }

    *tours = d->data.tours;
    return true;
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