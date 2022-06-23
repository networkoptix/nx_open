// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fisheye_home_ptz_controller.h"

#include <core/ptz/home_ptz_executor.h>
#include <core/ptz/ptz_controller_pool.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::client::desktop;

QnFisheyeHomePtzController::QnFisheyeHomePtzController(const QnPtzControllerPtr& baseController):
    QnHomePtzController(
        baseController,
        SystemContext::fromResource(
            baseController->resource())->ptzControllerPool()->executorThread()),
    m_suspended(false)
{
}

void QnFisheyeHomePtzController::suspend() {
    m_suspended = true;
    m_executor->stop();
}

void QnFisheyeHomePtzController::resume() {
    m_suspended = false;
    if (!m_executor->isRunning())
        restartExecutor();
}

void QnFisheyeHomePtzController::restartExecutor() {
    if (!m_suspended)
        base_type::restartExecutor();
}
