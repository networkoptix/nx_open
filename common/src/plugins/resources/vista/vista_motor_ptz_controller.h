#ifndef QN_VISTA_MOTOR_PTZ_CONTROLLER_H
#define QN_VISTA_MOTOR_PTZ_CONTROLLER_H

#include <core/ptz/basic_ptz_controller.h>

class CLSimpleHTTPClient;
class QnIniSection;

/**
 * Controller for vista-specific Motor PTZ functions, referred to in vista
 * docs as "MPTZ". Note that some of the vista PTZ cameras do not support 
 * this API.
 */
class QnVistaMotorPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnVistaMotorPtzController(const QnVistaResourcePtr &resource);
    virtual ~QnVistaMotorPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool continuousFocus(qreal speed) override;

private:
    CLSimpleHTTPClient *newHttpClient() const;
    bool query(const QString &request, QByteArray *body = NULL) const;
    bool query(const QString &request, QnIniSection *section, QByteArray *body = NULL);

    void init();

private:
    QnVistaResourcePtr m_resource;
    Qn::PtzCapabilities m_capabilities;
};

#endif // QN_VISTA_MOTOR_PTZ_CONTROLLER_H
