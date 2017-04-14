#pragma once

#include <core/resource/client_resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

struct QnLayoutTourItem
{
    QnLayoutResourcePtr layout;
    int delayMs = 0;

    QnLayoutTourItem() = default;
    QnLayoutTourItem(const QnLayoutResourcePtr& layout, int delayMs):
        layout(layout), delayMs(delayMs)
    {
    }

    QnLayoutTourItem(const ec2::ApiLayoutTourItemData& data, QnResourcePool* resourcePool);

    static QnLayoutTourItemList createList(const ec2::ApiLayoutTourItemDataList& items,
        QnResourcePool* resourcePool);
};
