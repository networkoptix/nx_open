#ifndef QN_RELATIVE_PTZ_CONTROLLER_H
#define QN_RELATIVE_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnRelativePtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnRelativePtzController(const QnPtzControllerPtr &baseController);
    
    //virtual Qn::PtzCapabilities getCapabilities() override;
    //virtual int relativeMove(const QRectF &viewport) override;
};


#endif // QN_RELATIVE_PTZ_CONTROLLER_H
