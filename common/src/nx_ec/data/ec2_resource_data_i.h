#ifndef QN_RESOURCE_DATA_I_H
#define QN_RESOURCE_DATA_I_H

#include "api_data_i.h"

namespace ec2 {

    struct ApiResourceParamData: ApiData
    {
        ApiResourceParamData() {}
        ApiResourceParamData(const QString& name, const QString& value, bool isResTypeParam): value(value), name(name), isResTypeParam(isResTypeParam) {}

        QString value;
        QString name;
        bool isResTypeParam;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiResourceParamData_Fields (value)(name)(isResTypeParam)


    struct ApiResourceData: ApiData {
        ApiResourceData(): status(QnResource::Offline), disabled(false) {}

        QnId          id;
        QnId          parentGuid;
        QnResource::Status    status;
        bool          disabled;
        QString       name;
        QString       url;
        QnId          typeId;
        std::vector<ApiResourceParamData> addParams;

        QN_DECLARE_STRUCT_SQL_BINDER();
    };

#define ApiResourceData_Fields (id)(parentGuid)(status)(disabled)(name)(url)(typeId)(addParams)

} // namespace ec2

#endif // QN_RESOURCE_DATA_I_H
