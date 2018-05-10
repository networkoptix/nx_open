#pragma once

#include "data.h"
#include "id_data.h"

namespace nx {
namespace vms {
namespace api {

struct PropertyTypeData: Data
{
    QnUuid resourceTypeId;

    QString name;
    QString defaultValue;
};
#define PropertyTypeData_Fields (resourceTypeId)(name)(defaultValue)

struct ResourceTypeData: IdData
{
    QString name;
    QString vendor;
    std::vector<QnUuid> parentId;
    std::vector<PropertyTypeData> propertyTypes;
};
#define ResourceTypeData_Fields IdData_Fields (name)(vendor)(parentId)(propertyTypes)

} // namespace api
} // namespace vms
} // namespace nx
