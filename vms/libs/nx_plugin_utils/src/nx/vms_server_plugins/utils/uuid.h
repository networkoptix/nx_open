#pragma once

#include <nx/utils/uuid.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace utils {

QnUuid fromSdkUuidToQnUuid(const nx::sdk::Uuid& guid);
nx::sdk::Uuid fromQnUuidToSdkUuid(const QnUuid& uuid);

} // namespace utils
} // namespace vms_server_plugins
} // namespace nx
