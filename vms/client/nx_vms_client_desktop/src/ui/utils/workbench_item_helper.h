// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/layout_item_data.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>
#include <recording/time_period.h>
#include <ui/workbench/workbench_item.h>

namespace helpers
{
    QnVirtualCameraResourcePtr extractCameraResource(QnWorkbenchItem *item);

    QnTimePeriod extractItemTimeWindow(QnWorkbenchItem *item);
}
