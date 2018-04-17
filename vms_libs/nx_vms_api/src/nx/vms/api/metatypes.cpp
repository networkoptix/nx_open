#include "metatypes.h"

#include <QtCore/QMetatype>

#include "data/database_dump_data.h"
#include "data/database_dump_to_file_data.h"
#include "data/resource_data.h"
#include "data/camera_data.h"

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

    qRegisterMetaType<nx::vms::api::DatabaseDumpData>();
    qRegisterMetaType<nx::vms::api::DatabaseDumpToFileData>();
    qRegisterMetaType<nx::vms::api::ResourceParamData>();
    qRegisterMetaType<nx::vms::api::ResourceParamDataList>();
    qRegisterMetaType<nx::vms::api::ResourceParamWithRefData>();
    qRegisterMetaType<nx::vms::api::ResourceParamWithRefDataList>();
    qRegisterMetaType<nx::vms::api::CameraData>();

};

} // namespace api
} // namespace vms
} // namespace nx
