#include "mapped_ptz_controller.h"

QnMappedPtzController::QnMappedPtzController(const QnPtzMapperPtr &mapper, const QnPtzControllerPtr &baseController):
    QnProxyPtzController(baseController),
    m_mapper(mapper)
{
    // TODO: #Elric proper finished handling.
}

bool QnMappedPtzController::extends(Ptz::Capabilities capabilities) {
    return 
        (capabilities & Ptz::Capability::AbsolutePtzCapabilities) &&
        (capabilities & Ptz::Capability::DevicePositioningPtzCapability) &&
        !(capabilities & Ptz::Capability::LogicalPositioningPtzCapability);
}

Ptz::Capabilities QnMappedPtzController::getCapabilities() {
    Ptz::Capabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Ptz::Capability::LogicalPositioningPtzCapability) : capabilities;
}

bool QnMappedPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(space == Qn::DevicePtzCoordinateSpace) {
        return base_type::absoluteMove(Qn::DevicePtzCoordinateSpace, position, speed);
    } else {
        return base_type::absoluteMove(Qn::DevicePtzCoordinateSpace, m_mapper->logicalToDevice(position), speed);
    }
}

bool QnMappedPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space == Qn::DevicePtzCoordinateSpace) {
        return base_type::getPosition(Qn::DevicePtzCoordinateSpace, position);
    } else {
        QVector3D devicePosition;
        if(!base_type::getPosition(Qn::DevicePtzCoordinateSpace, &devicePosition))
            return false;

        *position = m_mapper->deviceToLogical(devicePosition);
        return true;
    }
}

bool QnMappedPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    if(space == Qn::DevicePtzCoordinateSpace) {
        return base_type::getLimits(space, limits);
    } else {
        *limits = m_mapper->logicalLimits();
        return true;
    }
}

