// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>

#include <nx/sdk/helpers/action_response.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/ptr.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

nx::sdk::Ptr<nx::sdk::ActionResponse> generateActionResponse(
	const std::string& settingId,
	nx::sdk::Ptr<const nx::sdk::IStringMap> params,
	const std::map<std::string, std::string>& values);

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
