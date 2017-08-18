#pragma once

#include <nx/utils/uuid.h>
#include <plugins/plugin_api.h>

namespace nxpt {

QnUuid fromPluginGuidToQnUuid(nxpl::NX_GUID& guid);

} // namespace nxpt