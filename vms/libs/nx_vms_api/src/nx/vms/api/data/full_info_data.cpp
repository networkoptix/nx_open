// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "full_info_data.h"

#include "access_rights_data.h"
#include "camera_data.h"
#include "camera_attributes_data.h"
#include "camera_history_data.h"
#include "discovery_data.h"
#include "event_rule_data.h"
#include "layout_data.h"
#include "layout_tour_data.h"
#include "license_data.h"
#include "media_server_data.h"
#include "resource_type_data.h"
#include "user_data.h"
#include "user_role_data.h"
#include "videowall_data.h"
#include "webpage_data.h"
#include "analytics_data.h"
#include "../rules/rule.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FullInfoData, (ubjson)(json)(xml)(csv_record), FullInfoData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
