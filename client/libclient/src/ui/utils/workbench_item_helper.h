#pragma once

#include <ui/workbench/workbench_item.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>

#include <recording/time_period.h>

namespace helpers
{
    QnVirtualCameraResourcePtr extractCameraResource(QnWorkbenchItem *item);

    QnTimePeriod extractItemTimeWindow(QnWorkbenchItem *item);
}