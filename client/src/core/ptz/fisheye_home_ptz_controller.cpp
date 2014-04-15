#include "fisheye_home_ptz_controller.h"

#include <core/ptz/home_ptz_executor.h>

QnFisheyeHomePtzController::QnFisheyeHomePtzController(const QnPtzControllerPtr &baseController) :
    QnHomePtzController(baseController)
{
}

void QnFisheyeHomePtzController::suspend() {
    m_executor->stop();
}
