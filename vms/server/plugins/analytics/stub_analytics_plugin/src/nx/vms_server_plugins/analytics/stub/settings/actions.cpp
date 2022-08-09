// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "actions.h"

#include <nx/kit/utils.h>

#include "settings_model.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace settings {

nx::sdk::Ptr<nx::sdk::ActionResponse> generateActionResponse(
	const std::string& settingId,
	nx::sdk::Ptr<const nx::sdk::IStringMap> params)
{
    using namespace nx::sdk;

    if (settingId == kShowMessageButtonId)
    {
        const auto actionResponse = makePtr<ActionResponse>();
        const char* const param = params->value("parameter");
        actionResponse->setMessageToUser(
            "Message Example. \nParameter: " + nx::kit::utils::toString(param));

        return actionResponse;

    }

    if (settingId == kShowUrlButtonId)
    {
        const auto actionResponse = makePtr<ActionResponse>();
        actionResponse->setActionUrl("https://example.com/");
        return actionResponse;
    }

    return nullptr;
}

} // namespace settings
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
