#ifndef __EC2_VIDEOWALL_DATA_H_
#define __EC2_VIDEOWALL_DATA_H_

#include "ec2_resource_data.h"
#include "core/resource/resource_fwd.h"

namespace ec2
{

    struct ApiVideowall;

    #include "ec2_videowall_data_i.h"
    struct ApiVideowall: ApiVideowallData, ApiResource
    {
        void toResource(QnVideoWallResourcePtr resource) const;
        void fromResource(QnVideoWallResourcePtr resource);
        QN_DECLARE_STRUCT_SQL_BINDER();
    };

    QN_DEFINE_STRUCT_SQL_BINDER(ApiVideowall, ApiVideowallDataFields);

    struct ApiVideowallList: ApiVideowallListData
    {
        void loadFromQuery(QSqlQuery& query);
        template <class T> void toResourceList(QList<T>& outData) const;
    };
}

#endif // __EC2_VIDEOWALL_DATA_H_
