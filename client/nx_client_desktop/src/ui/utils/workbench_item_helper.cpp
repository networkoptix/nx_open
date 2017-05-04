#include "workbench_item_helper.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <core/resource/camera_resource.h>

namespace helpers {

template<typename ResourceType>
QnSharedResourcePointer<ResourceType> extractResource(QnWorkbenchItem *item)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    const auto layoutItemData = item->data();
    return resourcePool->getResourceByDescriptor(layoutItemData.resource).template dynamicCast<ResourceType>();

}

QnVirtualCameraResourcePtr extractCameraResource(QnWorkbenchItem *item)
{
    return extractResource<QnVirtualCameraResource>(item);
}

QnTimePeriod extractItemTimeWindow(QnWorkbenchItem *item)
{
    QnTimePeriod window = item->data(Qn::ItemSliderWindowRole).value<QnTimePeriod>();

    if(window.isEmpty())
        window = QnTimePeriod(QnTimePeriod::kMinTimeValue, QnTimePeriod::infiniteDuration());

    qint64 windowStartMs = window.startTimeMs;
    qint64 windowEndMs = (window.isInfinite() ? QnTimePeriod::kMaxTimeValue : window.endTimeMs());
    return QnTimePeriod::fromInterval(windowStartMs, windowEndMs);
}

} // namespace helpers
