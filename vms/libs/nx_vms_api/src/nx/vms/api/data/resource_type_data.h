// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "id_data.h"

#include <vector>

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PropertyTypeData
{
    QnUuid resourceTypeId;
    QString name;
    QString defaultValue;

    bool operator==(const PropertyTypeData& other) const = default;
};
#define PropertyTypeData_Fields (resourceTypeId)(name)(defaultValue)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(PropertyTypeData)

struct NX_VMS_API ResourceTypeData: IdData
{
    QString name;
    QString vendor;
    std::vector<QnUuid> parentId;
    PropertyTypeDataList propertyTypes;

    bool operator==(const ResourceTypeData& other) const = default;
};
#define ResourceTypeData_Fields IdData_Fields (name)(vendor)(parentId)(propertyTypes)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ResourceTypeData)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::PropertyTypeData)
Q_DECLARE_METATYPE(nx::vms::api::PropertyTypeDataList)
Q_DECLARE_METATYPE(nx::vms::api::ResourceTypeData)
Q_DECLARE_METATYPE(nx::vms::api::ResourceTypeDataList)
