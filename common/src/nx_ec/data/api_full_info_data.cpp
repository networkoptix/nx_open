#include "api_full_info_data.h"

#include <nx/vms/api/data/stored_file_data.h>
#include <nx_ec/data/api_user_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/api/data/webpage_data.h>
#include <nx_ec/data/api_discovery_data.h>
#include <nx_ec/data/api_system_name_data.h>
#include <nx_ec/data/api_peer_data.h>
#include <nx_ec/data/api_runtime_data.h>
#include <nx_ec/data/api_time_data.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/layout_tour_data.h>
#include <nx_ec/data/api_peer_system_time_data.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx_ec/data/api_user_role_data.h>

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiFullInfoData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

} // namespace ec2
