#pragma once

#include <nx/utils/uuid.h>
#include <plugins/plugin_api.h>

namespace nx {
namespace vms_server_plugins {
namespace utils {

QnUuid NX_PLUGIN_UTILS_API fromPluginGuidToQnUuid(const nxpl::NX_GUID& guid);
nxpl::NX_GUID NX_PLUGIN_UTILS_API fromQnUuidToPluginGuid(const QnUuid& uuid);

} // namespace utils
} // namespace vms_server_plugins
} // namespace nx
