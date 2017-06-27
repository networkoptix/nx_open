#ifndef __ABSTRACT_STORAGE_RESOURCE_TEST__
#define __ABSTRACT_STORAGE_RESOURCE_TEST__

#include <memory>
#include <algorithm>
#include <utility>
#include <cstring>
#include <string>
#include <random>

#include <nx/utils/thread/long_runnable.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/storage_manager.h>
#include <plugins/plugin_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <media_server/settings.h>
#include <core/resource_management/status_dictionary.h>
#include "utils/common/util.h"

#ifndef _WIN32
#   include <platform/monitoring/global_monitor.h>
#   include <platform/platform_abstraction.h>
#endif

namespace test
{
}
#endif
