#pragma once

#include <QtCore/QString>

#include <nx_ec/data/api_data.h>

#include <nx/utils/uuid.h>

namespace ec2 {

struct ApiLayoutTourItemData: ApiData
{
    QnUuid layoutId;
    int delayMs = 0;

    ApiLayoutTourItemData() = default;
    ApiLayoutTourItemData(const QnUuid& layoutId, int delayMs):
        layoutId(layoutId), delayMs(delayMs) {}

    bool operator==(const ApiLayoutTourItemData& rhs) const;
};

#define ApiLayoutTourItemData_Fields (layoutId)(delayMs)

struct ApiLayoutTourData: ApiData
{
    QnUuid id;
    QnUuid parentId;
    QString name;
    ApiLayoutTourItemDataList items;

    bool isValid() const;

    bool operator==(const ApiLayoutTourData& rhs) const;
    bool operator!=(const ApiLayoutTourData& rhs) const;
};

#define ApiLayoutTourData_Fields (id)(parentId)(name)(items)

} // namespace ec2
