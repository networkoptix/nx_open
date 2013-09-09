#ifndef QN_MAPPED_PTZ_CONTROLLER_H
#define QN_MAPPED_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnMappedPtzController: public QnProxyPtzController {
    Q_OBJECT;
public:
    QnMappedPtzController(const QnPtzControllerPtr &baseController):
        QnProxyPtzController(baseController)
    {
    }


private:
    QnPtzControllerPtr m_controller;
};


#endif // QN_MAPPED_PTZ_CONTROLLER_H
