#include "workbench_item_helper.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <core/resource/camera_resource.h>

QnVirtualCameraResourcePtr helpers::extractCameraResource(QnWorkbenchItem *item)
{
    return extractResource<QnVirtualCameraResource>(item);
}

QnTimePeriod helpers::extractItemTimeWindow(QnWorkbenchItem *item)
{
    QnTimePeriod window = item->data(Qn::ItemSliderWindowRole).value<QnTimePeriod>();

    if(window.isEmpty())
        window = QnTimePeriod(QnTimePeriod::kMinTimeValue, QnTimePeriod::infiniteDuration());

    qint64 windowStartMs = window.startTimeMs;
    qint64 windowEndMs = (window.isInfinite() ? QnTimePeriod::kMaxTimeValue : window.endTimeMs());
    return QnTimePeriod::fromInterval(windowStartMs, windowEndMs);
}
