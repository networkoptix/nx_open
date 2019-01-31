#include "mapped_ptz_controller.h"

#include <core/ptz/ptz_mapper.h>

QnMappedPtzController::QnMappedPtzController(const QnPtzMapperPtr &mapper, const QnPtzControllerPtr &baseController):
    QnProxyPtzController(baseController),
    m_mapper(mapper)
{
    // TODO: #Elric proper finished handling.
}

bool QnMappedPtzController::extends(Ptz::Capabilities capabilities) {
    return
        (capabilities & Ptz::AbsolutePtzCapabilities) &&
        (capabilities & Ptz::DevicePositioningPtzCapability) &&
        !(capabilities & Ptz::LogicalPositioningPtzCapability);
}

Ptz::Capabilities QnMappedPtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    return extends(capabilities) ? (capabilities | Ptz::LogicalPositioningPtzCapability) : capabilities;
}

bool QnMappedPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if(space == Qn::DevicePtzCoordinateSpace)
    {
        return base_type::absoluteMove(Qn::DevicePtzCoordinateSpace, position, speed, options);
    }
    else
    {
        return base_type::absoluteMove(
            Qn::DevicePtzCoordinateSpace,
            m_mapper->logicalToDevice(position),
            speed,
            options);
    }
}

bool QnMappedPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* position,
    const nx::core::ptz::Options& options) const
{
    if(space == Qn::DevicePtzCoordinateSpace)
    {
        return base_type::getPosition(Qn::DevicePtzCoordinateSpace, position, options);
    }
    else
    {
        nx::core::ptz::Vector devicePosition;
        if(!base_type::getPosition(Qn::DevicePtzCoordinateSpace, &devicePosition, options))
            return false;

        *position = m_mapper->deviceToLogical(devicePosition);
        return true;
    }
}

bool QnMappedPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const nx::core::ptz::Options& options) const
{
    if(space == Qn::DevicePtzCoordinateSpace) {
        return base_type::getLimits(space, limits, options);
    } else {
        *limits = m_mapper->logicalLimits();
        return true;
    }
}

