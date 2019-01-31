#pragma once

#include "id_data.h"

#include <QtCore/QString>

#include <nx/vms/api/types/resource_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API ResourceData: IdData
{
    QnUuid parentId;
    QString name;
    QString url;
    QnUuid typeId;

    ResourceData() = default;
    ResourceData(const QnUuid& typeId): typeId(typeId) {}

    static QnUuid getFixedTypeId(const QString& typeName);
};
#define ResourceData_Fields IdData_Fields (parentId)(name)(url)(typeId)

struct NX_VMS_API ResourceStatusData: IdData
{
    ResourceStatus status = ResourceStatus::offline;
};
#define ResourceStatusData_Fields IdData_Fields (status)

struct NX_VMS_API ResourceParamData: Data
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

struct NX_VMS_API ResourceParamWithRefData: ResourceParamData
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

Q_DECLARE_METATYPE(nx::vms::api::ResourceData)
Q_DECLARE_METATYPE(nx::vms::api::ResourceDataList)
Q_DECLARE_METATYPE(nx::vms::api::ResourceStatusData)
Q_DECLARE_METATYPE(nx::vms::api::ResourceStatusDataList)
Q_DECLARE_METATYPE(nx::vms::api::ResourceParamData)
Q_DECLARE_METATYPE(nx::vms::api::ResourceParamDataList)
Q_DECLARE_METATYPE(nx::vms::api::ResourceParamWithRefData)
Q_DECLARE_METATYPE(nx::vms::api::ResourceParamWithRefDataList)
