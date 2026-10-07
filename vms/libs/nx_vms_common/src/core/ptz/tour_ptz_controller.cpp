// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tour_ptz_controller.h"

#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/api/data/resource_property_key.h>

#include <api/resource_property_adaptor.h>

#include <core/ptz/tour_ptz_executor.h>
#include <core/resource/resource.h>

using namespace nx::core;

bool deserialize(const QString& /*value*/, QnPtzTourHash* /*target*/)
{
    NX_ASSERT(0, "Not implemented");
    return false;
}

QnTourPtzController::QnTourPtzController(
    const QnPtzControllerPtr& baseController, QThreadPool* threadPool, QThread* executorThread):
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzTourHash>(
        nx::vms::api::device_properties::kPtzTours, QnPtzTourHash(), this)),
    m_executor(new QnTourPtzExecutor(baseController, threadPool))
{
    NX_ASSERT(!baseController->hasCapabilities(Ptz::Capability::asynchronous));

    // TODO: #sivanov Implement it in a saner way.
    if (!baseController->hasCapabilities(Ptz::Capability::virtual_))
        m_executor->moveToThread(executorThread);

    m_adaptor->setResource(baseController->resource());
    connect(
        m_adaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        [this] { emit changed(DataField::tours); },
        Qt::QueuedConnection);

    connect(m_adaptor,
        &QnAbstractResourcePropertyAdaptor::synchronizationNeeded,
        this,
        [](const QnResourcePtr& resource)
        {
            if (NX_ASSERT(resource))
                resource->savePropertiesAsync();
        });
}

QnTourPtzController::~QnTourPtzController() = default;

bool QnTourPtzController::extends(Ptz::Capabilities capabilities)
{
    return capabilities.testFlag(Ptz::Capability::presets)
        && !capabilities.testFlag(Ptz::Capability::tours);
}

Ptz::Capabilities QnTourPtzController::getCapabilities(const Options& options) const
{
    Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    if (options.type != Type::operational)
        return capabilities;

    return extends(capabilities) ? (capabilities | Ptz::Capability::tours) : capabilities;
}

bool QnTourPtzController::continuousMove(const Vector& speed, const Options& options)
{
    if (!supports(Command::continuousMove, options))
        return false;

    stopActiveTour();
    return base_type::continuousMove(speed, options);
}

bool QnTourPtzController::absoluteMove(
    CoordinateSpace space, const Vector& position, qreal speed, const Options& options)
{
    if (!supports(spaceCommand(Command::absoluteDeviceMove, space), options))
        return false;

    stopActiveTour();
    return base_type::absoluteMove(space, position, speed, options);
}

bool QnTourPtzController::viewportMove(
    qreal aspectRatio, const QRectF& viewport, qreal speed, const Options& options)
{
    if (!supports(Command::viewportMove, options))
        return false;

    stopActiveTour();
    return base_type::viewportMove(aspectRatio, viewport, speed, options);
}

bool QnTourPtzController::activatePreset(const QString& presetId, qreal speed)
{
    // Preset activation is always supported by a controller extended with tours.
    stopActiveTour();
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
        auto lockedActiveTour = m_activeTour.lock();
        QnPtzTourHash records = m_adaptor->value();
        if (records.contains(tour.id) && records.value(tour.id) == tour)
            return true; //< No need to save an unchanged tour.

        records.insert(tour.id, tour);

        if (lockedActiveTour->id == tour.id)
        {
            activeTour = tour;
            activeTour.optimize();

            if (activeTour != *lockedActiveTour)
            {
                restartTour = true;
                *lockedActiveTour = activeTour;
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
        auto lockedActiveTour = m_activeTour.lock();

        QnPtzTourHash records = m_adaptor->value();
        if (records.remove(tourId) == 0)
            return false;

        if (lockedActiveTour->id == tourId)
        {
            *lockedActiveTour = QnPtzTour();
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
        auto lockedActiveTour = m_activeTour.lock();
        if (lockedActiveTour->id == tourId)
            return true; //< The requested tour is already active.

        const QnPtzTourHash& records = m_adaptor->value();
        if (!records.contains(tourId))
            return false;

        activeTour = records.value(tourId);
        activeTour.optimize();

        *lockedActiveTour = activeTour;
    }

    if (activeTour.isValid(presets))
        m_executor->startTour(activeTour);

    return true;
}

std::optional<QnPtzTour> QnTourPtzController::getActiveTour()
{
    return *m_activeTour.lock();
}

bool QnTourPtzController::getTours(QnPtzTourList* tours) const
{
    *tours = m_adaptor->value().values();
    return true;
}

void QnTourPtzController::stopActiveTour()
{
    m_executor->stopTour();

    *m_activeTour.lock() = QnPtzTour();
}
