#include "tour_ptz_controller.h"

#include <QtCore/QMetaObject>

#include <nx/utils/thread/mutex.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/thread/long_runnable.h>

#include <api/resource_property_adaptor.h>

#include <core/ptz/tour_ptz_executor.h>
#include <core/ptz/ptz_controller_pool.h>

bool deserialize(const QString& /*value*/, QnPtzTourHash* /*target*/)
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "Not implemented");
    return false;
}

QnTourPtzController::QnTourPtzController(
    const QnPtzControllerPtr &baseController,
    QThreadPool* threadPool,
    QThread* executorThread):
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzTourHash>(
        lit("ptzTours"), QnPtzTourHash(), this)),
    m_executor(new QnTourPtzExecutor(baseController, threadPool))
{
    NX_ASSERT(!baseController->hasCapabilities(Ptz::AsynchronousPtzCapability)); // TODO: #Elric

    // TODO: #Elric implement it in a saner way
    if (!baseController->hasCapabilities(Ptz::VirtualPtzCapability))
        m_executor->moveToThread(executorThread);

    m_adaptor->setResource(baseController->resource());
    connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this,
        [this]{ emit changed(Qn::ToursPtzField); }, Qt::QueuedConnection);
}

QnTourPtzController::~QnTourPtzController()
{
    m_executor->deleteLater();
}

bool QnTourPtzController::extends(Ptz::Capabilities capabilities)
{
    return capabilities.testFlag(Ptz::PresetsPtzCapability)
        && !capabilities.testFlag(Ptz::ToursPtzCapability);
}

Ptz::Capabilities QnTourPtzController::getCapabilities() const
{
    Ptz::Capabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Ptz::ToursPtzCapability) : capabilities;
}

bool QnTourPtzController::continuousMove(const QVector3D& speed)
{
    if (!supports(Qn::ContinuousMovePtzCommand))
        return false;

    clearActiveTour();
    return base_type::continuousMove(speed);
}

bool QnTourPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const QVector3D& position,
    qreal speed)
{
    if (!supports(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space)))
        return false;

    clearActiveTour();
    return base_type::absoluteMove(space, position, speed);
}

bool QnTourPtzController::viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed)
{
    if (!supports(Qn::ViewportMovePtzCommand))
        return false;

    clearActiveTour();
    return base_type::viewportMove(aspectRatio, viewport, speed);
}

bool QnTourPtzController::activatePreset(const QString& presetId, qreal speed)
{
    /* This one is 100% supported, no need to check. */

    clearActiveTour();
    return base_type::activatePreset(presetId, speed);
}

bool QnTourPtzController::createTour(const QnPtzTour& tour)
{
    QnPtzPresetList presets;
    if (!getPresets(&presets))
        return false;

    bool restartTour = false;
    QnPtzTour activeTour;
    {
        const QnMutexLocker locker(&m_mutex);
        QnPtzTourHash records = m_adaptor->value();
        if (records.contains(tour.id) && records.value(tour.id) == tour)
            return true; /* No need to save it. */

        records.insert(tour.id, tour);

        if (m_activeTour.id == tour.id)
        {
            activeTour = tour;
            activeTour.optimize();

            if (activeTour != m_activeTour)
            {
                restartTour = true;
                m_activeTour = activeTour;
            }
        }

        m_adaptor->setValue(records);
    }

    if (restartTour)
    {
        m_executor->stopTour();
        if (activeTour.isValid(presets))
            m_executor->startTour(activeTour);
    }

    emit changed(Qn::ToursPtzField);
    return true;
}

bool QnTourPtzController::removeTour(const QString& tourId)
{
    bool stopTour = false;
    {
        const QnMutexLocker locker(&m_mutex);

        QnPtzTourHash records = m_adaptor->value();
        if (records.remove(tourId) == 0)
            return false;

        if (m_activeTour.id == tourId)
        {
            m_activeTour = QnPtzTour();
            stopTour = true;
        }

        m_adaptor->setValue(records);
    }

    if (stopTour)
        m_executor->stopTour();

    emit changed(Qn::ToursPtzField);
    return true;
}

bool QnTourPtzController::activateTour(const QString& tourId)
{
    QnPtzPresetList presets;
    if (!getPresets(&presets))
        return false;

    QnPtzTour activeTour;
    {
        const QnMutexLocker locker(&m_mutex);
        if (m_activeTour.id == tourId)
            return true; /* Already activated. */

        const QnPtzTourHash &records = m_adaptor->value();
        if (!records.contains(tourId))
            return false;

        activeTour = records.value(tourId);
        activeTour.optimize();

        m_activeTour = activeTour;
    }

    if (activeTour.isValid(presets))
        m_executor->startTour(activeTour);

    return true;
}

bool QnTourPtzController::getTours(QnPtzTourList* tours) const
{
    *tours = m_adaptor->value().values();
    return true;
}

void QnTourPtzController::clearActiveTour()
{
    m_executor->stopTour();

    const QnMutexLocker locker(&m_mutex);
    m_activeTour = QnPtzTour();
}
