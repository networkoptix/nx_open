#pragma once

#include <QtCore/QString>

#include <nx_ec/data/api_data.h>

#include <nx/utils/uuid.h>

namespace ec2 {

struct ApiLayoutTourItemData: ApiData
{
    QnUuid resourceId;
    int delayMs = 0;

    ApiLayoutTourItemData() = default;
    ApiLayoutTourItemData(const QnUuid& resourceId, int delayMs):
        resourceId(resourceId), delayMs(delayMs) {}

    bool operator==(const ApiLayoutTourItemData& rhs) const;
};

#define ApiLayoutTourItemData_Fields (resourceId)(delayMs)

struct ApiLayoutTourSettings: ApiData
{
    bool manual = false;

    bool operator==(const ApiLayoutTourSettings& rhs) const;
    bool operator!=(const ApiLayoutTourSettings& rhs) const;
};
#define ApiLayoutTourSettings_Fields (manual)

struct ApiLayoutTourData: ApiData
{
    QnUuid id;
    QnUuid parentId;
    QString name;
    ApiLayoutTourItemDataList items;
    ApiLayoutTourSettings settings;

    bool isValid() const;

    bool operator==(const ApiLayoutTourData& rhs) const;
    bool operator!=(const ApiLayoutTourData& rhs) const;
};

#define ApiLayoutTourData_Fields (id)(parentId)(name)(items)(settings)

} // namespace ec2

// Support SQL via JSON (de)serialization
void serialize_field(const ec2::ApiLayoutTourSettings& settings, QVariant* target);
void deserialize_field(const QVariant& value, ec2::ApiLayoutTourSettings* target);
