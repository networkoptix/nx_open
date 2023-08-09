// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_helper.h"

namespace nx::vms::client::core {

QnUuid IntercomHelper::intercomOpenDoorRuleId(const QnUuid& cameraId)
{
    static const std::string kOpenDoorRuleIdBase = "nx.sys.IntercomIntegrationOpenDoor";
    return QnUuid::fromArbitraryData(kOpenDoorRuleIdBase + cameraId.toStdString());
}

} // namespace nx::vms::client::core
