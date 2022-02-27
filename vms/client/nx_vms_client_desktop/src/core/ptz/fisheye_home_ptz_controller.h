// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef FISHEYE_HOME_PTZ_CONTROLLER_H
#define FISHEYE_HOME_PTZ_CONTROLLER_H

#include <core/ptz/home_ptz_controller.h>

class QnFisheyeHomePtzController : public QnHomePtzController {
    Q_OBJECT
    typedef QnHomePtzController base_type;
public:
    QnFisheyeHomePtzController(const QnPtzControllerPtr &baseController);

public slots:
    void suspend();
    void resume();

protected:
    virtual void restartExecutor() override;

private:
    bool m_suspended;
};

#endif // FISHEYE_HOME_PTZ_CONTROLLER_H
