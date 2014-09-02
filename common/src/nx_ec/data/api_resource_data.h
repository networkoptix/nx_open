#ifndef __RESOURCE_TRANSACTION_DATA_H__
#define __RESOURCE_TRANSACTION_DATA_H__

#include "api_globals.h"
#include "api_data.h"


namespace ec2
{
    struct ApiResourceParamData: ApiData
    {
        ApiResourceParamData() {}
        ApiResourceParamData(const QString& name, const QString& value, bool predefinedParam): value(value), name(name), predefinedParam(predefinedParam) {}

        QString value;
        QString name;
        bool predefinedParam;
    };
#define ApiResourceParamData_Fields (value)(name)(predefinedParam)


    struct ApiResourceParamWithRefData: ApiResourceParamData
    {
        QUuid resourceId;
    };
#define ApiResourceParamWithRefData_Fields ApiResourceParamData_Fields (resourceId)


    struct ApiResourceParamsData: ApiData
    {
        QUuid id;
        std::vector<ApiResourceParamData> params;
    };
#define ApiResourceParamsData_Fields (id)(params)


    struct ApiResourceData: ApiData {
        ApiResourceData(): status(Qn::Offline) {}

        QUuid          id;
        QUuid          parentId;
        Qn::ResourceStatus    status;
        QString       name;
        QString       url;
        QUuid          typeId;
        std::vector<ApiResourceParamData> addParams;
    };
#define ApiResourceData_Fields (id)(parentId)(status)(name)(url)(typeId)(addParams)

/*
    struct ApiSetResourceDisabledData: ApiData {
        QUuid id;
        bool disabled;
    };
#define ApiSetResourceDisabledData_Fields (id)(disabled)
*/

    struct ApiSetResourceStatusData: ApiData
    {
        QUuid id;
        Qn::ResourceStatus status;
    };
#define ApiSetResourceStatusData_Fields (id)(status)

} // namespace ec2

#endif // __RESOURCE_TRANSACTION_DATA_H__
