#pragma once

#include <core/resource/resource_fwd.h>

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
