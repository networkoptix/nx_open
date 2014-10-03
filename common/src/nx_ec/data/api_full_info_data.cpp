#include "api_full_info_data.h"
#include "api_model_functions_impl.h"

#include "api_stored_file_data.h"
#include "api_user_data.h"
#include "api_videowall_data.h"
#include "api_update_data.h"
#include "api_module_data.h"
#include "api_discovery_data.h"
#include "api_routing_data.h"
#include "api_system_name_data.h"
#include "api_peer_data.h"
#include "api_runtime_data.h"
#include "api_time_data.h"
#include "api_license_overflow_data.h"
#include "api_media_server_data.h"
#include "api_resource_type_data.h"
#include "api_license_data.h"
#include "api_business_rule_data.h"
#include "api_camera_data.h"
#include "api_camera_server_item_data.h"
#include "api_camera_bookmark_data.h"
#include "api_email_data.h"
#include "api_layout_data.h"
#include "api_peer_system_time_data.h"


namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiFullInfoData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)
} // namespace ec2
