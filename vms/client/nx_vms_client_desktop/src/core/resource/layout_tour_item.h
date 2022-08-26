// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource/resource_fwd.h>

struct QnLayoutTourItem
{
    nx::vms::client::desktop::LayoutResourcePtr layout;
    int delayMs = 0;

    QnLayoutTourItem() = default;
    QnLayoutTourItem(const nx::vms::client::desktop::LayoutResourcePtr& layout, int delayMs):
        layout(layout), delayMs(delayMs)
    {
    }
};
