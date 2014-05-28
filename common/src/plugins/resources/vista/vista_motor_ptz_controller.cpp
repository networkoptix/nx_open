#include "vista_motor_ptz_controller.h"

QnVistaMotorPtzController::QnVistaMotorPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_capabilities(Qn::NoPtzCapabilities)
{

}

QnVistaMotorPtzController::~QnVistaMotorPtzController() {
    return;
}

Qn::PtzCapabilities QnVistaMotorPtzController::getCapabilities() {
    return base_type::getCapabilities() | m_capabilities;
}

bool QnVistaMotorPtzController::continuousMove(const QVector3D &speed) {
    return base_type::continuousMove(speed);
}

bool QnVistaMotorPtzController::continuousFocus(qreal speed) {
    return base_type::continuousFocus(speed);
}
