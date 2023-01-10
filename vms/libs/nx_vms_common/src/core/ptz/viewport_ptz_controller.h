// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_VIEWPORT_PTZ_CONTROLLER_H
#define QN_VIEWPORT_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class NX_VMS_COMMON_API QnViewportPtzController: public QnProxyPtzController
{
    typedef QnProxyPtzController base_type;

public:
    QnViewportPtzController(const QnPtzControllerPtr &baseController);

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override;
    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const Options& options) override;
};


#endif // QN_VIEWPORT_PTZ_CONTROLLER_H
