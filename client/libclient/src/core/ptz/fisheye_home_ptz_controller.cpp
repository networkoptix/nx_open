#include "fisheye_home_ptz_controller.h"

#include <core/ptz/home_ptz_executor.h>

QnFisheyeHomePtzController::QnFisheyeHomePtzController(const QnPtzControllerPtr &baseController) :
    QnHomePtzController(baseController),
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
