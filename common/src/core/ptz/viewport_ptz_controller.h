#ifndef QN_VIEWPORT_PTZ_CONTROLLER_H
#define QN_VIEWPORT_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnViewportPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnViewportPtzController(const QnPtzControllerPtr &baseController);
    
    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities() const override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) override;
};


#endif // QN_VIEWPORT_PTZ_CONTROLLER_H
