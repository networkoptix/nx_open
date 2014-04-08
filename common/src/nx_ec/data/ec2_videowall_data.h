#ifndef __EC2_VIDEOWALL_DATA_H_
#define __EC2_VIDEOWALL_DATA_H_

#include "ec2_resource_data.h"
#include "core/resource/resource_fwd.h"

namespace ec2
{

    struct ApiVideowallData: public ApiResourceData
    {
        ApiVideowallData(): autorun(false) {}
    
        bool autorun;

        void toResource(QnVideoWallResourcePtr resource) const;
        void fromResource(QnVideoWallResourcePtr resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    #define ApiVideowallDataFields (autorun)
    QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS(ApiVideowallData, ec2::ApiResourceData, ApiVideowallDataFields)


    struct ApiVideowallDataList: public ApiData
    {
        std::vector<ApiVideowallData> data;

        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };

    QN_DEFINE_STRUCT_SERIALIZATORS (ApiVideowallDataList, (data) )
}

#endif // __EC2_VIDEOWALL_DATA_H_
