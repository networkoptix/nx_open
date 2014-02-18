#include "tour_ptz_controller.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>

#include <utils/common/json.h>
#include <utils/common/long_runnable.h>

#include <api/resource_property_adaptor.h>

#include "tour_ptz_executor.h"
#include "ptz_controller_pool.h"

// -------------------------------------------------------------------------- //
// QnTourPtzController
// -------------------------------------------------------------------------- //
QnTourPtzController::QnTourPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzTourHash>(baseController->resource(), lit("ptzTours"), QnPtzTourHash(), this)),
    m_executor(new QnTourPtzExecutor(baseController))
{
    assert(qnPtzPool); /* Ptz pool must exist as it hosts executor thread. */
    assert(!baseController->hasCapabilities(Qn::AsynchronousPtzCapability)); // TODO: #Elric

    m_executor->moveToThread(qnPtzPool->executorThread());

    connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChangedExternally, this, [this]{ emit changed(Qn::ToursPtzField); }, Qt::QueuedConnection);
}

QnTourPtzController::~QnTourPtzController() {
    m_executor->deleteLater();
}

bool QnTourPtzController::extends(Qn::PtzCapabilities capabilities) {
    return 
        (capabilities & Qn::PresetsPtzCapability) &&
        ((capabilities & Qn::AbsolutePtzCapabilities) == Qn::AbsolutePtzCapabilities) &&
        (capabilities & (Qn::DevicePositioningPtzCapability | Qn::LogicalPositioningPtzCapability)) &&
        !(capabilities & Qn::ToursPtzCapability);
}

Qn::PtzCapabilities QnTourPtzController::getCapabilities() {
    Qn::PtzCapabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Qn::ToursPtzCapability) : capabilities;
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
    QnPtzPresetList presets;
    if(!getPresets(&presets))
        return false;

    bool restartTour = false;
    QnPtzTour activeTour;
    {
        QMutexLocker locker(&m_mutex);
        QnPtzTourHash records = m_adaptor->value();
        if(records.contains(tour.id) && records.value(tour.id) == tour)
            return true; /* No need to save it. */

        records.insert(tour.id, tour);

        if(m_activeTour.id == tour.id) {
            activeTour = tour;
            activeTour.optimize();
            
            if(activeTour != m_activeTour) {
                restartTour = true;
                m_activeTour = activeTour;
            }
        }

        m_adaptor->setValue(records);
    }

    if(restartTour) {
        m_executor->stopTour();
        m_executor->startTour(activeTour);
    }

    emit changed(Qn::ToursPtzField);
    return true;
}

bool QnTourPtzController::removeTour(const QString &tourId) {
    bool stopTour = false;
    {
        QMutexLocker locker(&m_mutex);

        QnPtzTourHash records = m_adaptor->value();
        if(records.remove(tourId) == 0)
            return false;
    
        if(m_activeTour.id == tourId) {
            m_activeTour = QnPtzTour(); // TODO: #Elric this won't be registered by activity controller.
            stopTour = true;
        }

        m_adaptor->setValue(records);
    }

    if(stopTour)
        m_executor->stopTour();
    
    emit changed(Qn::ToursPtzField);
    return true;
}

bool QnTourPtzController::activateTour(const QString &tourId) {
    QnPtzTour activeTour;
    {
        QMutexLocker locker(&m_mutex);
        const QnPtzTourHash &records = m_adaptor->value();
        if(!records.contains(tourId))
            return false;

        activeTour = records.value(tourId);
        activeTour.optimize();

        m_activeTour = activeTour;
    }

    m_executor->startTour(activeTour);

    return true;
}

bool QnTourPtzController::getTours(QnPtzTourList *tours) {
    QMutexLocker locker(&m_mutex);

    *tours = m_adaptor->value().values();

    return true;
}

