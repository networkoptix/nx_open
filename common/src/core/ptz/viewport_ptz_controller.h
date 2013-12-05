#ifndef QN_VIEWPORT_PTZ_CONTROLLER_H
#define QN_VIEWPORT_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnViewportPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnViewportPtzController(const QnPtzControllerPtr &baseController);
    
    static bool extends(const QnPtzControllerPtr &baseController);

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport) override;
};


#endif // QN_VIEWPORT_PTZ_CONTROLLER_H
