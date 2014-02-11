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
#include "ec2_resource_type_data.h"

namespace ec2 
{
    struct ApiFullData: public ApiData 
    {
        ApiResourceTypeList resTypes;
        ApiMediaServerDataList servers;
        ApiCameraDataList cameras;
        ApiUserDataList users;
        ApiLayoutDataList layouts;
        ApiBusinessRuleDataList rules;
        ApiCameraServerItemDataList cameraHistory;
        ServerInfo serverInfo;
        
        void toResourceList(QnFullResourceData&, const ResourceContext&) const;
    };
}

QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ServerInfo, (hardwareId1) (oldHardwareId) (hardwareId2) 
    (publicIp) (systemName) (hardwareId3) (sessionKey) (allowCameraChanges) )
QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiFullData, (resTypes) (servers) (cameras) (users) (layouts) (rules) (cameraHistory) (serverInfo) )

#endif // __EC2_FULL_DATA_H_
