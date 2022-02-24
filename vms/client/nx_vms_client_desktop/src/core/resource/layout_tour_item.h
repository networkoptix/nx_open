// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/client_resource_fwd.h>

struct QnLayoutTourItem
{
    QnLayoutResourcePtr layout;
    int delayMs = 0;

    QnLayoutTourItem() = default;
    QnLayoutTourItem(const QnLayoutResourcePtr& layout, int delayMs):
        layout(layout), delayMs(delayMs)
    {
    }
};
