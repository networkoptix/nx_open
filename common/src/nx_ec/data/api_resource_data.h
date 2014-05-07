#ifndef __RESOURCE_TRANSACTION_DATA_H__
#define __RESOURCE_TRANSACTION_DATA_H__

#include "api_globals.h"
#include "api_data.h"


namespace ec2
{
    struct ApiResourceParamData: ApiData
    {
        ApiResourceParamData() {}
        ApiResourceParamData(const QString& name, const QString& value, bool isResTypeParam): value(value), name(name), isResTypeParam(isResTypeParam) {}

        QString value;
        QString name;
        bool isResTypeParam; // TODO: #API rename into something more sane. Right now it's not clear what is this and I don't know what to rename it into. 
    };
#define ApiResourceParamData_Fields (value)(name)(isResTypeParam)


    struct ApiResourceParamWithRefData: ApiResourceParamData
    {
        QnId resourceId;
    };
#define ApiResourceParamWithRefData_Fields ApiResourceParamData_Fields (resourceId)


    struct ApiResourceParamsData: ApiData
    {
        QnId id;
        std::vector<ApiResourceParamData> params;
    };
#define ApiResourceParamsData_Fields (id)(params)


    struct ApiResourceData: ApiData {
        ApiResourceData(): status(QnResource::Offline), disabled(false) {}

        QnId          id;
        QnId          parentId;
        QnResource::Status    status;
        bool          disabled; // TODO: #API I thought we got rid of this one?
        QString       name;
        QString       url;
        QnId          typeId;
        std::vector<ApiResourceParamData> addParams;
    };
#define ApiResourceData_Fields (id)(parentId)(status)(disabled)(name)(url)(typeId)(addParams)


    struct ApiSetResourceDisabledData: ApiData {
        QnId id;
        bool disabled; // TODO: #API huh?
    };
#define ApiSetResourceDisabledData_Fields (id)(disabled)


    struct ApiSetResourceStatusData: ApiData
    {
        QnId id;
        QnResource::Status status;
    };
#define ApiSetResourceStatusData_Fields (id)(status)

} // namespace ec2

#endif // __RESOURCE_TRANSACTION_DATA_H__
