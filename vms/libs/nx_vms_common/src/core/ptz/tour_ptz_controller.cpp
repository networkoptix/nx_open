// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tour_ptz_controller.h"

#include <QtCore/QMetaObject>

#include <api/resource_property_adaptor.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/tour_ptz_executor.h>
#include <core/resource/resource.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>

using namespace nx::core;

const QString QnTourPtzController::kTourPropertyName = "ptzTours";

bool deserialize(const QString& /*value*/, QnPtzTourHash* /*target*/)
{
    NX_ASSERT(0, "Not implemented");
    return false;
}

QnTourPtzController::QnTourPtzController(
    const QnPtzControllerPtr &baseController,
    QThreadPool* threadPool,
    QThread* executorThread)
    :
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzTourHash>(kTourPropertyName, QnPtzTourHash(), this)),
    m_executor(new QnTourPtzExecutor(baseController, threadPool))
{
    NX_ASSERT(!baseController->hasCapabilities(Ptz::AsynchronousPtzCapability));

    // TODO: #sivanov Implement it in a saner way.
    if (!baseController->hasCapabilities(Ptz::VirtualPtzCapability))
        m_executor->moveToThread(executorThread);

    connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this,
        [this](const QString& key, const QString& value)
        {
            if (auto r = resource(); NX_ASSERT(r) && r->setProperty(key, value))
            {
                emit changed(DataField::tours);
                r->savePropertiesAsync();
            }
        },
        Qt::QueuedConnection);

    if (auto resource = baseController->resource())
        m_adaptor->loadValue(resource->getProperty(m_adaptor->key()));
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

Ptz::Capabilities QnTourPtzController::getCapabilities(const Options& options) const
{
    Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    if (options.type != Type::operational)
        return capabilities;

    return extends(capabilities) ? (capabilities | Ptz::ToursPtzCapability) : capabilities;
}

bool QnTourPtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    if (!supports(Command::continuousMove, options))
        return false;

    clearActiveTour();
    return base_type::continuousMove(speed, options);
}

bool QnTourPtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    if (!supports(spaceCommand(Command::absoluteDeviceMove, space), options))
        return false;

    clearActiveTour();
    return base_type::absoluteMove(space, position, speed, options);
}

bool QnTourPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    if (!supports(Command::viewportMove, options))
        return false;

    clearActiveTour();
    return base_type::viewportMove(aspectRatio, viewport, speed, options);
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
        const NX_MUTEX_LOCKER locker(&m_mutex);
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

    emit changed(DataField::tours);
    return true;
}

bool QnTourPtzController::removeTour(const QString& tourId)
{
    bool stopTour = false;
    {
        const NX_MUTEX_LOCKER locker(&m_mutex);

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

    emit changed(DataField::tours);
    return true;
}

bool QnTourPtzController::activateTour(const QString& tourId)
{
    QnPtzPresetList presets;
    if (!getPresets(&presets))
        return false;

    QnPtzTour activeTour;
    {
        const NX_MUTEX_LOCKER locker(&m_mutex);
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

std::optional<QnPtzTour> QnTourPtzController::getActiveTour()
{
    return m_activeTour;
}

bool QnTourPtzController::getTours(QnPtzTourList* tours) const
{
    *tours = m_adaptor->value().values();
    return true;
}

void QnTourPtzController::clearActiveTour()
{
    m_executor->stopTour();

    const NX_MUTEX_LOCKER locker(&m_mutex);
    m_activeTour = QnPtzTour();
}
