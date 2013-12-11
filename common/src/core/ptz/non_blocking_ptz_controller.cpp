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
    QnPtzCommand(const QnPtzControllerPtr &controller, Qn::PtzAction action, const Functor &functor):
        QnAbstractPtzCommand(controller, action),
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

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->continuousMove(speed);
    });
    return true;
}

bool QnNonBlockingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::AbsolutePtzCapabilities) || !hasSpaceCapabilities(capabilities, space))
        return false;

    runCommand(Qn::NoPtzFields, [=](const QnPtzControllerPtr &controller, QVariant *) -> bool {
        return controller->absoluteMove(space, position, speed);
    });
    return true;
}

bool QnNonBlockingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(!(capabilities & Qn::ViewportPtzCapability))
        return false;

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


}

bool QnNonBlockingPtzController::updatePreset(const QnPtzPreset &preset) {
    return true;
}

bool QnNonBlockingPtzController::removePreset(const QString &presetId) {
    return true;
}

bool QnNonBlockingPtzController::activatePreset(const QString &presetId, qreal speed) {
    return true;
}

bool QnNonBlockingPtzController::getPresets(QnPtzPresetList *presets) {
    return true;
}

bool QnNonBlockingPtzController::createTour(const QnPtzTour &tour) {
    return true;
}

bool QnNonBlockingPtzController::removeTour(const QString &tourId) {
    return true;
}

bool QnNonBlockingPtzController::activateTour(const QString &tourId) {
    return true;
}

bool QnNonBlockingPtzController::getTours(QnPtzTourList *tours) {
    return true;
}

void QnNonBlockingPtzController::synchronize(Qn::PtzDataFields fields) {
    
}
