#ifndef __EC2_FULL_DATA_H_
#define __EC2_FULL_DATA_H_

#include "ec2_resource_data.h"
#include "camera_data.h"
#include "mserver_data.h"
#include "ec2_business_rule_data.h"
#include "ec2_user_data.h"
#include "ec2_layout_data.h"
#include "nx_ec/ec_api.h"
#include "camera_server_item_data.h"

namespace ec2 
{
    struct QnFullResourceData
    {
        QnResourceList resources;
        QnBusinessEventRuleList bRules;
        QnCameraHistoryList cameraHistory;
        QnKvPairList kvPairs;
    };

    struct ApiFullData: public ApiData 
    {
        ApiMediaServerDataList servers;
        ApiCameraDataList cameras;
        ApiUserDataList users;
        ApiLayoutDataList layouts;
        ApiBusinessRuleDataList rules;
        ApiCameraServerItemDataList cameraHistory;
        ApiResourceParams kvPairs;
        
        void toResourceList(QnFullResourceData&, const ResourceContext&) const;
    };
}

QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiFullData, (servers) (cameras) (users) (layouts) (rules) (cameraHistory) )

#endif // __EC2_FULL_DATA_H_
