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

struct ApiResourceData: ApiIdData
{
    ApiResourceData() {}

    QnUuid parentId;
    QString name;
    QString url;
    QnUuid typeId;

    bool operator==(const ApiResourceData&) const;
};
#define ApiResourceData_Fields ApiIdData_Fields (parentId)(name)(url)(typeId)

struct ApiResourceStatusData: ApiIdData
{
    ApiResourceStatusData(): status(Qn::Offline) {}

    Qn::ResourceStatus status;
};
#define ApiResourceStatusData_Fields ApiIdData_Fields (status)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiResourceParamData)
Q_DECLARE_METATYPE(ec2::ApiResourceParamWithRefData)
