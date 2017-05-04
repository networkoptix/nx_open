#include "api_full_info_data.h"

#include <nx_ec/data/api_stored_file_data.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/data/api_videowall_data.h>
#include <nx_ec/data/api_webpage_data.h>
#include <nx_ec/data/api_update_data.h>
#include <nx_ec/data/api_discovery_data.h>
#include <nx_ec/data/api_routing_data.h>
#include <nx_ec/data/api_system_name_data.h>
#include <nx_ec/data/api_peer_data.h>
#include <nx_ec/data/api_runtime_data.h>
#include <nx_ec/data/api_time_data.h>
#include <nx_ec/data/api_license_overflow_data.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/data/api_resource_type_data.h>
#include <nx_ec/data/api_license_data.h>
#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_camera_data.h>
#include <nx_ec/data/api_camera_attributes_data.h>
#include <nx_ec/data/api_camera_history_data.h>
#include <nx_ec/data/api_email_data.h>
#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/data/api_layout_tour_data.h>
#include <nx_ec/data/api_peer_system_time_data.h>
#include <nx_ec/data/api_access_rights_data.h>
#include <nx_ec/data/api_user_role_data.h>

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiFullInfoData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

} // namespace ec2
