#include "mapped_ptz_controller.h"

#include <cassert>

QnMappedPtzController::QnMappedPtzController(const QnPtzMapperPtr &mapper, const QnPtzControllerPtr &baseController):
    QnProxyPtzController(baseController),
    m_mapper(mapper)
{
    assert(baseController->hasCapabilities(Qn::AbsolutePtzCapabilities));
}

Qn::PtzCapabilities QnMappedPtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::LogicalPositionSpaceCapability;
}

int QnMappedPtzController::absoluteMove(const QVector3D &position) {
    return base_type::absoluteMove(m_mapper->logicalToDevice(position));
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

