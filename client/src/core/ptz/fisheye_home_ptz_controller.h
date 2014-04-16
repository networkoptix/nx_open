#ifndef FISHEYE_HOME_PTZ_CONTROLLER_H
#define FISHEYE_HOME_PTZ_CONTROLLER_H

#include <core/ptz/home_ptz_controller.h>

class QnFisheyeHomePtzController : public QnHomePtzController {
    Q_OBJECT
public:
    QnFisheyeHomePtzController(const QnPtzControllerPtr &baseController);

public slots:
    void suspend();
};

#endif // FISHEYE_HOME_PTZ_CONTROLLER_H
