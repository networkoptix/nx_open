#include "activity_ptz_controller.h"

#include <api/resource_property_adaptor.h>

#include <core/resource/resource.h>
#include <core/resource_management/resource_properties.h>


QnActivityPtzController::QnActivityPtzController(
    Mode mode,
    const QnPtzControllerPtr& baseController)
    :
    base_type(baseController),
    m_mode(mode)
{
    m_adaptor = new QnJsonResourcePropertyAdaptor<QnPtzObject>(
        lit("ptzActiveObject"), QnPtzObject(), this);
    connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this,
        [this]() { emit changed(Qn::ActiveObjectPtzField); });

    /* Adaptor is thread-safe and works even without resource,
     * exactly what we need for local mode. */
    if (m_mode == Local)
        return;

    m_adaptor->setResource(resource());

    connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::synchronizationNeeded, this,
        [this](const QnResourcePtr& resource)
        {
            propertyDictionary->saveParamsAsync(resource->getId());
        });
}

QnActivityPtzController::~QnActivityPtzController()
{
}

bool QnActivityPtzController::extends(Ptz::Capabilities capabilities)
{
    return (capabilities.testFlag(Ptz::PresetsPtzCapability)
        || capabilities.testFlag(Ptz::ToursPtzCapability))
        && !capabilities.testFlag(Ptz::ActivityPtzCapability);
}

Ptz::Capabilities QnActivityPtzController::getCapabilities() const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Ptz::ActivityPtzCapability) : capabilities;
}

bool QnActivityPtzController::continuousMove(const QVector3D& speed)
{
    if (!base_type::continuousMove(speed))
        return false;

    if (m_mode != Client)
        m_adaptor->setValue(QnPtzObject());
    return true;
}

bool QnActivityPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const QVector3D& position,
    qreal speed)
{
    if (!base_type::absoluteMove(space, position, speed))
        return false;

    if (m_mode != Client)
        m_adaptor->setValue(QnPtzObject());
    return true;
}

bool QnActivityPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed)
{
    if (!base_type::viewportMove(aspectRatio, viewport, speed))
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

bool QnActivityPtzController::removeTour(const QString &tourId) {
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

bool QnActivityPtzController::getData(Qn::PtzDataFields query, QnPtzData* data) const
{
    // TODO: #Elric #PTZ this is a hack. Need to do it better.
    return baseController()->hasCapabilities(Ptz::AsynchronousPtzCapability)
        ? baseController()->getData(query, data)
        : base_type::getData(query, data);
}

