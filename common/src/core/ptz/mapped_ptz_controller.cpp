#include "mapped_ptz_controller.h"

QnMappedPtzController::QnMappedPtzController(const QnPtzMapperPtr &mapper, const QnPtzControllerPtr &baseController):
    QnProxyPtzController(baseController),
    m_mapper(mapper)
{
    m_mapper->
}

virtual Qn::PtzCapabilities QnMappedPtzController::getCapabilities() override {
    return base_type::getCapabilities() | Qn::LogicalPositionSpaceCapability;
}

virtual int QnMappedPtzController::setPosition(const QVector3D &position) override {
    return base_type::setPosition(m_mapper->);
}

virtual int QnMappedPtzController::getPosition(QVector3D *position) override {
    
}

virtual int QnMappedPtzController::getLimits(QnPtzLimits *limits) override {
    
}
