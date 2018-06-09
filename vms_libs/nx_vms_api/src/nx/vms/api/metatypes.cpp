#include "metatypes.h"

#include <QtCore/QMetaType>

#include "data/access_rights_data.h"
#include "data/database_dump_data.h"
#include "data/database_dump_to_file_data.h"
#include "data/resource_data.h"
#include "data/camera_data.h"
#include "data/camera_attributes_data.h"
#include "data/camera_history_data.h"
#include "data/cloud_system_data.h"
#include "data/webpage_data.h"
#include "data/layout_data.h"
#include "data/layout_tour_data.h"
#include "data/license_data.h"
#include "data/license_overflow_data.h"
#include "data/lock_data.h"
#include "data/update_data.h"
#include "data/videowall_data.h"
#include "data/event_rule_data.h"
#include "data/cleanup_db_data.h"

namespace {

static bool nx_vms_api_metatypes_initialized = false;

} // namespace

namespace nx {
namespace vms {
namespace api {

void Metatypes::initialize()
{
    // Note that running the code twice is perfectly OK.
    if (nx_vms_api_metatypes_initialized)
        return;

    nx_vms_api_metatypes_initialized = true;

    // Fully qualified namespaces are mandatory here and in all signals declarations.

    qRegisterMetaType<nx::vms::api::AccessRightsData>();
    qRegisterMetaType<nx::vms::api::AccessRightsDataList>();
    qRegisterMetaType<nx::vms::api::CameraAttributesData>();
    qRegisterMetaType<nx::vms::api::CameraData>();
    qRegisterMetaType<nx::vms::api::CameraHistoryData>();
    qRegisterMetaType<nx::vms::api::CameraHistoryDataList>();
    qRegisterMetaType<nx::vms::api::CameraHistoryItemData>();
    qRegisterMetaType<nx::vms::api::CameraHistoryItemDataList>();
    qRegisterMetaType<nx::vms::api::CleanupDatabaseData>();
    qRegisterMetaType<nx::vms::api::CloudSystemData>();
    qRegisterMetaType<nx::vms::api::CloudSystemDataList>();
    qRegisterMetaType<nx::vms::api::DatabaseDumpData>();
    qRegisterMetaType<nx::vms::api::DatabaseDumpToFileData>();
    qRegisterMetaType<nx::vms::api::DetailedLicenseData>();
    qRegisterMetaType<nx::vms::api::EventRuleData>();
    qRegisterMetaType<nx::vms::api::EventRuleDataList>();
    qRegisterMetaType<nx::vms::api::LayoutData>();
    qRegisterMetaType<nx::vms::api::LayoutItemData>();
    qRegisterMetaType<nx::vms::api::LayoutTourData>();
    qRegisterMetaType<nx::vms::api::LicenseData>();
    qRegisterMetaType<nx::vms::api::LicenseOverflowData>();
    qRegisterMetaType<nx::vms::api::LockData>();
    qRegisterMetaType<nx::vms::api::ResourceData>();
    qRegisterMetaType<nx::vms::api::ResourceStatusData>();
    qRegisterMetaType<nx::vms::api::ResourceParamData>();
    qRegisterMetaType<nx::vms::api::ResourceParamDataList>();
    qRegisterMetaType<nx::vms::api::ResourceParamWithRefData>();
    qRegisterMetaType<nx::vms::api::ResourceParamWithRefDataList>();
    qRegisterMetaType<nx::vms::api::ServerFootageData>();
    qRegisterMetaType<nx::vms::api::ServerFootageDataList>();
    qRegisterMetaType<nx::vms::api::UpdateInstallData>();
    qRegisterMetaType<nx::vms::api::UpdateUploadData>();
    qRegisterMetaType<nx::vms::api::UpdateUploadResponseData>();
    qRegisterMetaType<nx::vms::api::VideowallData>();
    qRegisterMetaType<nx::vms::api::VideowallControlMessageData>();
    qRegisterMetaType<nx::vms::api::WebPageData>();
};

} // namespace api
} // namespace vms
} // namespace nx
