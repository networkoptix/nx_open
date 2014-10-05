#ifndef __RESOURCE_TRANSACTION_DATA_H__
#define __RESOURCE_TRANSACTION_DATA_H__

#include "api_globals.h"
#include "api_data.h"


namespace ec2
{
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
        ApiResourceParamWithRefData(const QnUuid& resourceId, const QString& name, const QString& value): resourceId(resourceId), ApiResourceParamData(name, value) {}
        QnUuid resourceId;
    };
#define ApiResourceParamWithRefData_Fields ApiResourceParamData_Fields (resourceId)


    struct ApiResourceData: ApiData {
        ApiResourceData(): status(Qn::Offline) {}

        QnUuid          id;
        QnUuid          parentId;
        Qn::ResourceStatus    status;
        QString       name;
        QString       url;
        QnUuid          typeId;
    };
#define ApiResourceData_Fields (id)(parentId)(status)(name)(url)(typeId)

    struct ApiSetResourceStatusData: ApiData
    {
        QnUuid id;
        Qn::ResourceStatus status;
    };
#define ApiSetResourceStatusData_Fields (id)(status)

} // namespace ec2

#endif // __RESOURCE_TRANSACTION_DATA_H__
