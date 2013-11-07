#include "mapped_ptz_controller.h"

QnMappedPtzController::QnMappedPtzController(const QnPtzMapperPtr &mapper, const QnPtzControllerPtr &baseController):
    QnProxyPtzController(baseController),
    m_mapper(mapper)
{}

Qn::PtzCapabilities QnMappedPtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::LogicalPositionSpaceCapability;
}

int QnMappedPtzController::setPosition(const QVector3D &position) {
    return base_type::setPosition(m_mapper->logicalToDevice(position));
}

int QnMappedPtzController::getPosition(QVector3D *position) {
    QVector3D devicePosition;
    int result = base_type::getPosition(&devicePosition);
    if(result != 0)
        return result;
    
    *position = m_mapper->deviceToLogical(devicePosition);
    return result;
}

int QnMappedPtzController::getLimits(QnPtzLimits *limits) {
    *limits = m_mapper->logicalLimits();
    return 0;
}

