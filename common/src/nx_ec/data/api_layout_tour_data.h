#pragma once

#include <QtCore/QString>

#include <nx_ec/data/api_data.h>

#include <nx/utils/uuid.h>

namespace ec2 {

struct ApiLayoutTourItemData: ApiData
{
    QnUuid layoutId;
    int delayMs = 0;
};

#define ApiLayoutTourItemData_Fields ApiData_Fields (layoutId)(delayMs)

struct ApiLayoutTourItemWithRefData: ApiLayoutTourItemData
{
    QnUuid tourId;
};
#define ApiLayoutTourItemWithRefData_Fields ApiLayoutTourItemData_Fields (tourId)

struct ApiLayoutTourData: public ApiIdData
{
    QString name;
    std::vector<ApiLayoutTourItemData> items;
};

#define ApiLayoutTourData_Fields ApiIdData_Fields (name)(items)

} // namespace ec2
