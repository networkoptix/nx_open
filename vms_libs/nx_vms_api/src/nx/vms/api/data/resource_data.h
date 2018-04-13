#pragma once

#include "id_data.h"

#include <nx/vms/api/types/resource_types.h>

namespace nx {
namespace vms {
namespace api {

struct ResourceData: IdData
{
    QnUuid parentId;
    QString name;
    QString url;
    QnUuid typeId;
};
#define ResourceData_Fields IdData_Fields (parentId)(name)(url)(typeId)

struct ResourceStatusData: IdData
{
    ResourceStatus status = ResourceStatus::offline;
};
#define ResourceStatusData_Fields IdData_Fields (status)

struct ResourceParamData: Data
{
    ResourceParamData() = default;

    ResourceParamData(const QString& name, const QString& value):
        value(value),
        name(name)
    {
    }

    QString value;
    QString name;
};
#define ResourceParamData_Fields (value)(name)

struct ResourceParamWithRefData: ResourceParamData
{
    ResourceParamWithRefData() = default;

    ResourceParamWithRefData(
        const QnUuid& resourceId,
        const QString& name,
        const QString& value)
        :
        ResourceParamData(name, value),
        resourceId(resourceId)
    {
    }

    QnUuid resourceId;
};
#define ResourceParamWithRefData_Fields ResourceParamData_Fields (resourceId)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::ResourceParamData)
Q_DECLARE_METATYPE(nx::vms::api::ResourceParamWithRefData)
