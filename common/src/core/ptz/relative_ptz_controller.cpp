#include "relative_ptz_controller.h"

#include <cassert>

QnRelativePtzController::QnRelativePtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController) 
{
    assert(baseController->hasCapabilities(Qn::LogicalPositionSpaceCapability));
}

Qn::PtzCapabilities QnRelativePtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::ScreenSpaceMovementCapability;
}

int QnRelativePtzController::relativeMove(qreal aspectRatio, const QRectF &viewport) {
    return 0;
}
