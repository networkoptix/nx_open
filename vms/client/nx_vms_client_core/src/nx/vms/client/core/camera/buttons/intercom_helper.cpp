// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_helper.h"

namespace nx::vms::client::core {

nx::Uuid IntercomHelper::intercomOpenDoorRuleId(const nx::Uuid& cameraId)
{
    static const std::string kOpenDoorRuleIdBase = "nx.sys.IntercomIntegrationOpenDoor";
    return nx::Uuid::fromArbitraryData(kOpenDoorRuleIdBase + cameraId.toStdString());
}

} // namespace nx::vms::client::core
