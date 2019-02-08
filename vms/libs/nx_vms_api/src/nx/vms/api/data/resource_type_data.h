#pragma once

#include "id_data.h"

#include <vector>

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PropertyTypeData: Data
{
    QnUuid resourceTypeId;

    QString name;
    QString defaultValue;
};
#define PropertyTypeData_Fields (resourceTypeId)(name)(defaultValue)

struct NX_VMS_API ResourceTypeData: IdData
{
    QString name;
    QString vendor;
    std::vector<QnUuid> parentId;
    PropertyTypeDataList propertyTypes;
};
#define ResourceTypeData_Fields IdData_Fields (name)(vendor)(parentId)(propertyTypes)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::PropertyTypeData)
Q_DECLARE_METATYPE(nx::vms::api::PropertyTypeDataList)
Q_DECLARE_METATYPE(nx::vms::api::ResourceTypeData)
Q_DECLARE_METATYPE(nx::vms::api::ResourceTypeDataList)
