// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "activity_ptz_controller.h"

#include <api/resource_property_adaptor.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_properties.h>
#include <nx/utils/qt_direct_connect.h>

using namespace nx::core;

QnActivityPtzController::QnActivityPtzController(
    Mode mode,
    const QnPtzControllerPtr& baseController)
    :
    base_type(baseController),
    m_mode(mode),
    m_adaptor(
        new QnJsonResourcePropertyAdaptor<QnPtzObject>("ptzActiveObject", QnPtzObject(), this))
{
    /* Adaptor is thread-safe and works even without resource,
     * exactly what we need for local mode. */
    if (m_mode == Local)
        return;

    auto resource = this->resource();
    if (!NX_ASSERT(resource))
        return;

    m_connections = {
        nx::qtDirectConnect(resource.get(), &QnResource::propertyChanged,
            [this](const auto&, const auto& key, const auto&, const auto& value)
            {
                if (key == m_adaptor->key())
                    m_adaptor->loadValue(value);
            }),
        nx::qtDirectConnect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
            [this](const QString& key, const QString& value)
            {
                if (auto r = this->resource(); NX_ASSERT(r) && r->setProperty(key, value))
                {
                    emit changed(DataField::activeObject);
                    r->savePropertiesAsync();
                }
            })
    };
    m_adaptor->loadValue(resource->getProperty(m_adaptor->key()));
}

bool QnActivityPtzController::extends(Ptz::Capabilities capabilities)
{
    return (capabilities.testFlag(Ptz::PresetsPtzCapability)
        || capabilities.testFlag(Ptz::ToursPtzCapability))
        && !capabilities.testFlag(Ptz::ActivityPtzCapability);
}

Ptz::Capabilities QnActivityPtzController::getCapabilities(
    const Options& options) const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    if (options.type != Type::operational)
        return capabilities;

    return extends(capabilities) ? (capabilities | Ptz::ActivityPtzCapability) : capabilities;
}

bool QnActivityPtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    if (!base_type::continuousMove(speed, options))
        return false;

    if (m_mode != Client)
        m_adaptor->setValue(QnPtzObject());
    return true;
}

bool QnActivityPtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    if (!base_type::absoluteMove(space, position, speed, options))
        return false;

    if (m_mode != Client)
        m_adaptor->setValue(QnPtzObject());
    return true;
}

bool QnActivityPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    if (!base_type::viewportMove(aspectRatio, viewport, speed, options))
        return false;

    if (m_mode != Client)
        m_adaptor->setValue(QnPtzObject());
    return true;
}

bool QnActivityPtzController::removePreset(const QString& presetId)
{
    if (!base_type::removePreset(presetId))
        return false;

    m_adaptor->testAndSetValue(QnPtzObject(Qn::PresetPtzObject, presetId), QnPtzObject());
    return true;
}

bool QnActivityPtzController::activatePreset(const QString& presetId, qreal speed)
{
    if (!base_type::activatePreset(presetId, speed))
        return false;

    if (m_mode != Client)
        m_adaptor->setValue(QnPtzObject(Qn::PresetPtzObject, presetId));

    return true;
}

bool QnActivityPtzController::removeTour(const QString &tourId)
{
    if (!base_type::removeTour(tourId))
        return false;

    m_adaptor->testAndSetValue(QnPtzObject(Qn::TourPtzObject, tourId), QnPtzObject());
    return true;
}

bool QnActivityPtzController::activateTour(const QString& tourId)
{
    if (!base_type::activateTour(tourId))
        return false;

    if (m_mode != Client)
        m_adaptor->setValue(QnPtzObject(Qn::TourPtzObject, tourId));
    return true;
}

bool QnActivityPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    *activeObject = m_adaptor->value();
    return true;
}

bool QnActivityPtzController::getData(
    QnPtzData* data,
    DataFields query,
    const Options& options) const
{
    return baseController()->hasCapabilities(Ptz::AsynchronousPtzCapability, options)
        ? baseController()->getData(data, query, options)
        : base_type::getData(data, query, options);
}
