#ifndef QN_WORKAROUND_PTZ_CONTROLLER_H
#define QN_WORKAROUND_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnWorkaroundPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnWorkaroundPtzController(const QnPtzControllerPtr &baseController);

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities() const override;
    virtual bool continuousMove(const nx::core::ptz::Vector& speed) override;

private:
    bool m_overrideContinuousMove;
    Qt::Orientations m_flip;
    Ptz::Traits m_traits;

    bool m_overrideCapabilities;
    Ptz::Capabilities m_capabilities;
    Ptz::Capabilities m_capabilitiesToAdd;
    Ptz::Capabilities m_capabilitiesToRemove;
};

#endif // QN_WORKAROUND_PTZ_CONTROLLER_H
