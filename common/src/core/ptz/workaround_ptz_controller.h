#ifndef QN_WORKAROUND_PTZ_CONTROLLER_H
#define QN_WORKAROUND_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnWorkaroundPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnWorkaroundPtzController(const QnPtzControllerPtr &baseController);

    static bool extends(Qn::PtzCapabilities capabilities);

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool continuousMove(const QVector3D &speed) override;

private:
    bool m_overrideContinuousMove;
    Qt::Orientations m_flip;
    Qn::PtzTraits m_traits;

    bool m_overrideCapabilities;
    Qn::PtzCapabilities m_capabilities;
};

#endif // QN_WORKAROUND_PTZ_CONTROLLER_H
