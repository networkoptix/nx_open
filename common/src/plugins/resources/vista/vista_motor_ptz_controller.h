#ifndef QN_VISTA_MOTOR_PTZ_CONTROLLER_H
#define QN_VISTA_MOTOR_PTZ_CONTROLLER_H

#include <core/ptz/proxy_ptz_controller.h>

/**
 * Controller for vista-specific Motor PTZ functions, referred to in vista
 * docs as "MPTZ". Note that some of the vista PTZ cameras do not support 
 * this API.
 */
class QnVistaMotorPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnVistaMotorPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnVistaMotorPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool continuousFocus(qreal speed) override;

private:
    Qn::PtzCapabilities m_capabilities;
};

#endif // QN_VISTA_MOTOR_PTZ_CONTROLLER_H
