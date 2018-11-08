#include "fisheye_home_ptz_controller.h"

#include <client_core/client_core_module.h>

#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/home_ptz_executor.h>

QnFisheyeHomePtzController::QnFisheyeHomePtzController(const QnPtzControllerPtr &baseController) :
    QnHomePtzController(baseController, qnClientCoreModule->ptzControllerPool()->executorThread()),
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
