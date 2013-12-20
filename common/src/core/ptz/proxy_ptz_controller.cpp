#include "proxy_ptz_controller.h"

#include <cassert>

#include <utils/common/event_loop.h>

QnProxyPtzController::QnProxyPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController->resource()),
    m_baseController(baseController)
{
    assert(qnHasEventLoop(thread()));

    connect(baseController, &QnAbstractPtzController::finished,             this, &QnProxyPtzController::baseFinished);
    connect(baseController, &QnAbstractPtzController::capabilitiesChanged,  this, &QnProxyPtzController::baseCapabilitiesChanged);
}

QnProxyPtzController::~QnProxyPtzController() {
    return;
}

