#include "tour_ptz_controller.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>

#include <utils/common/json.h>
#include <utils/common/long_runnable.h>

#include <api/resource_property_adaptor.h>

#include "ptz_tour_executor.h"


// -------------------------------------------------------------------------- //
// QnPtzTourExecutorThread
// -------------------------------------------------------------------------- //
class QnPtzTourExecutorThread: public QnLongRunnable {
    typedef QnLongRunnable base_type;
public:
    QnPtzTourExecutorThread() {
        start();
    }

    virtual ~QnPtzTourExecutorThread() {
        stop();
    }

    virtual void pleaseStop() override {
        base_type::pleaseStop();

        QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
    }

    virtual void run() override {
        /* Default implementation is OK, but we want to see this function on 
         * the stack when debugging. */
        return base_type::run(); 
    }
};

Q_GLOBAL_STATIC(QnPtzTourExecutorThread, qn_ptzTourExecutorThread_instance);


// -------------------------------------------------------------------------- //
// QnTourPtzController
// -------------------------------------------------------------------------- //
QnTourPtzController::QnTourPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzTourHash>(baseController->resource(), lit("ptzTours"), this)),
    m_executor(new QnPtzTourExecutor(baseController))
{
    m_executor->moveToThread(qn_ptzTourExecutorThread_instance());
}

QnTourPtzController::~QnTourPtzController() {
    return;
}

bool QnTourPtzController::extends(const QnPtzControllerPtr &baseController) {
    return 
        baseController->hasCapabilities(Qn::PresetsPtzCapability) &&
        baseController->hasCapabilities(Qn::AbsolutePtzCapabilities) &&
        (baseController->hasCapabilities(Qn::DevicePositioningPtzCapability) || baseController->hasCapabilities(Qn::LogicalPositioningPtzCapability)) &&
        !baseController->hasCapabilities(Qn::ToursPtzCapability);
}

Qn::PtzCapabilities QnTourPtzController::getCapabilities() {
    /* Note that this controller preserves the Qn::NonBlockingPtzCapability. */
    return baseController()->getCapabilities() | Qn::ToursPtzCapability;
}

bool QnTourPtzController::continuousMove(const QVector3D &speed) {
    if(!supports(Qn::ContinuousMovePtzCommand))
        return false;

    m_executor->stopTour();
    return base_type::continuousMove(speed);
}

bool QnTourPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!supports(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space)))
        return false;
    
    m_executor->stopTour();
    return base_type::absoluteMove(space, position, speed);
}

bool QnTourPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!supports(Qn::ViewportMovePtzCommand))
        return false;

    m_executor->stopTour();
    return base_type::viewportMove(aspectRatio, viewport, speed);
}

bool QnTourPtzController::activatePreset(const QString &presetId, qreal speed) {
    /* This one is 100% supported, no need to check. */
    m_executor->stopTour();
    return base_type::activatePreset(presetId, speed);
}

bool QnTourPtzController::createTour(const QnPtzTour &tour) {
    return createTourInternal(tour);
}

bool QnTourPtzController::createTourInternal(QnPtzTour tour) {
    QnPtzPresetList presets;
    if(!getPresets(&presets))
        return false;

    /* We need to check validity of the tour first. */
    if (!tour.isValid(presets))
        return false;

    /* Not so important so fix and continue. */
    tour.optimize();

    /* Tour is fine, save it. */
    QMutexLocker locker(&m_mutex);
    QnPtzTourHash records = m_adaptor->value();
    records.insert(tour.id, tour);
    
    m_adaptor->setValue(records);
    return true;
}

bool QnTourPtzController::removeTour(const QString &tourId) {
    QMutexLocker locker(&m_mutex);

    QnPtzTourHash records = m_adaptor->value();
    if(records.remove(tourId) == 0)
        return false;
    
    m_adaptor->setValue(records);
    return true;
}

bool QnTourPtzController::activateTour(const QString &tourId) {
    QnPtzTour tour;
    {
        QMutexLocker locker(&m_mutex);
        const QnPtzTourHash &records = m_adaptor->value();
        if(!records.contains(tourId))
            return false;
        tour = records.value(tourId);
    }

    m_executor->startTour(tour);
    return true;
}

bool QnTourPtzController::getTours(QnPtzTourList *tours) {
    QMutexLocker locker(&m_mutex);

    *tours = m_adaptor->value().values();
    return true;
}
