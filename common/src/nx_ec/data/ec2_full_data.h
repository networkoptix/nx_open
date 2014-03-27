#ifndef __EC2_FULL_DATA_H_
#define __EC2_FULL_DATA_H_

#include "ec2_resource_data.h"
#include "camera_data.h"
#include "mserver_data.h"
#include "ec2_business_rule_data.h"
#include "ec2_user_data.h"
#include "ec2_layout_data.h"
#include "ec2_license.h"
#include "nx_ec/ec_api.h"
#include "camera_server_item_data.h"
#include "ec2_resource_type_data.h"
#include "ec2_email.h"

namespace ec2 
{
    #include "ec2_full_data_i.h"

    struct ApiFullInfo: public ApiFullInfoData
    {
        void toResourceList(QnFullResourceData&, const ResourceContext&) const;
    };
}

#endif // __EC2_FULL_DATA_H_
