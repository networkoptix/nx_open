#include "proxy_ptz_controller.h"

#include <cassert>

QnProxyPtzController::QnProxyPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController->resource()),
    m_baseController(baseController)
{
    connect(baseController, &QnAbstractPtzController::finished,             this, &QnProxyPtzController::baseFinished);
    connect(baseController, &QnAbstractPtzController::changed,              this, &QnProxyPtzController::baseChanged);
    connect(this,           &QnProxyPtzController::finishedLater,           this, &QnAbstractPtzController::finished, Qt::QueuedConnection);
}

QnProxyPtzController::~QnProxyPtzController() {
    return;
}

