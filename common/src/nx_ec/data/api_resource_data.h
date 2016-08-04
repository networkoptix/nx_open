#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

struct ApiResourceParamData: ApiData
{
    ApiResourceParamData() {}
    ApiResourceParamData(const QString& name, const QString& value): value(value), name(name) {}

    QString value;
    QString name;
};
#define ApiResourceParamData_Fields (value)(name)
Q_DECLARE_METATYPE(ApiResourceParamData)

struct ApiResourceParamWithRefData: ApiResourceParamData
{
    ApiResourceParamWithRefData() {}

    ApiResourceParamWithRefData(
        const QnUuid& resourceId, const QString& name, const QString& value)
        :
        ApiResourceParamData(name, value), resourceId(resourceId)
    {
    }

    QnUuid resourceId;
};
#define ApiResourceParamWithRefData_Fields ApiResourceParamData_Fields (resourceId)
Q_DECLARE_METATYPE(ApiResourceParamWithRefData)

struct ApiResourceData: ApiData
{
    ApiResourceData() {}

    QnUuid id;
    QnUuid parentId;
    QString name;
    QString url;
    QnUuid typeId;
};
#define ApiResourceData_Fields (id)(parentId)(name)(url)(typeId)

struct ApiResourceStatusData: ApiData
{
    ApiResourceStatusData(): status(Qn::Offline) {}

    QnUuid id;
    Qn::ResourceStatus status;
};
#define ApiResourceStatusData_Fields (id)(status)

} // namespace ec2
