#pragma once

#include <nx/utils/uuid.h>
#include <plugins/plugin_api.h>

namespace nxpt {

QnUuid fromPluginGuidToQnUuid(const nxpl::NX_GUID& guid);

} // namespace nxpt

namespace nx {
namespace sdk {

QString toString(const nx::sdk::ResourceInfo& resourceInfo);

} // namespace sdk
} // namespace nx
