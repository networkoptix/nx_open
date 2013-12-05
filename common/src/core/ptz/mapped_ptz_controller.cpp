#include "mapped_ptz_controller.h"

QnMappedPtzController::QnMappedPtzController(const QnPtzMapperPtr &mapper, const QnPtzControllerPtr &baseController):
    QnProxyPtzController(baseController),
    m_mapper(mapper)
{}

bool QnMappedPtzController::extends(const QnPtzControllerPtr &baseController) {
    return 
        baseController->hasCapabilities(Qn::AbsolutePtzCapabilities | Qn::DeviceCoordinateSpaceCapability) &&
        !baseController->hasCapabilities(Qn::LogicalCoordinateSpaceCapability);
}

Qn::PtzCapabilities QnMappedPtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::LogicalCoordinateSpaceCapability;
}

bool QnMappedPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) {
    if(space == Qn::DeviceCoordinateSpace) {
        return base_type::absoluteMove(Qn::DeviceCoordinateSpace, position);
    } else {
        return base_type::absoluteMove(Qn::DeviceCoordinateSpace, m_mapper->logicalToDevice(position));
    }
}

bool QnMappedPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space == Qn::DeviceCoordinateSpace) {
        return base_type::getPosition(Qn::DeviceCoordinateSpace, position);
    } else {
        QVector3D devicePosition;
        if(!base_type::getPosition(Qn::DeviceCoordinateSpace, &devicePosition))
            return false;

        *position = m_mapper->deviceToLogical(devicePosition);
        return true;
    }
}

bool QnMappedPtzController::getLimits(QnPtzLimits *limits) {
    *limits = m_mapper->logicalLimits();
    return true;
}

