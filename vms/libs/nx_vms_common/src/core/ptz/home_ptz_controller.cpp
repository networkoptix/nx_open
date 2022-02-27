// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "home_ptz_controller.h"

#include <cassert>

#include <QtCore/QCoreApplication>

#include <api/resource_property_adaptor.h>

#include <utils/common/invocation_event.h>

#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/home_ptz_executor.h>

using namespace nx::core;

QnHomePtzController::QnHomePtzController(
    const QnPtzControllerPtr &baseController,
    QThread* executorThread)
    :
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzObject>(
        "ptzHomeObject", QnPtzObject(), this)),
    m_executor(new QnHomePtzExecutor(baseController))
{
    NX_ASSERT(!baseController->hasCapabilities(Ptz::AsynchronousPtzCapability));
    m_adaptor->setResource(baseController->resource());
    m_executor->moveToThread(executorThread);

    connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
        this, &QnHomePtzController::at_adaptor_valueChanged);

    at_adaptor_valueChanged();
    restartExecutor();
}

QnHomePtzController::~QnHomePtzController()
{
    m_executor->deleteLater();
}

bool QnHomePtzController::extends(Ptz::Capabilities capabilities)
{
    return (capabilities.testFlag(Ptz::PresetsPtzCapability)
        || capabilities.testFlag(Ptz::ToursPtzCapability))
        && !capabilities.testFlag(Ptz::HomePtzCapability);
}

Ptz::Capabilities QnHomePtzController::getCapabilities(const Options& options) const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    if (options.type != Type::operational)
        return capabilities;

    return extends(capabilities) ? (capabilities | Ptz::HomePtzCapability) : capabilities;
}

bool QnHomePtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    if(!base_type::continuousMove(speed, options))
        return false;

    restartExecutor();
    return true;
}

bool QnHomePtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    if(!base_type::absoluteMove(space, position, speed, options))
        return false;

    restartExecutor();
    return true;
}

bool QnHomePtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    if(!base_type::viewportMove(aspectRatio, viewport, speed, options))
        return false;

    restartExecutor();
    return true;
}

bool QnHomePtzController::activatePreset(const QString& presetId, qreal speed)
{
    if(!base_type::activatePreset(presetId, speed))
        return false;

    restartExecutor();
    return true;
}

bool QnHomePtzController::activateTour(const QString& tourId)
{
    if(!base_type::activateTour(tourId))
        return false;

    m_executor->stop();
    return true;
}

bool QnHomePtzController::updateHomeObject(const QnPtzObject& homeObject)
{
    const Ptz::Capabilities capabilities = getCapabilities({Type::operational});
    if(homeObject.type == Qn::PresetPtzObject && !capabilities.testFlag(Ptz::PresetsPtzCapability))
        return false;

    if(homeObject.type == Qn::TourPtzObject && !capabilities.testFlag(Ptz::ToursPtzCapability))
        return false;

    m_adaptor->setValue(homeObject);
    return true;
}

bool QnHomePtzController::getHomeObject(QnPtzObject* homeObject) const
{
    *homeObject = m_adaptor->value();
    return true;
}

void QnHomePtzController::restartExecutor()
{
    m_executor->restart();
}

void QnHomePtzController::at_adaptor_valueChanged()
{
    // Only operational PTZ is supported now.
    m_executor->setHomePosition(m_adaptor->value());

    /* Restart only if it's running right now. */
    if(m_executor->isRunning())
        restartExecutor();

    emit changed(DataField::homeObject);
}
