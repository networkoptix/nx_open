// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_cloud_system_finder.h"

#include <nx/fusion/serialization/json_functions.h>
#include <nx/network/rest/result.h>

namespace nx::vms::client::core::test {

TestCloudSystemFinder::TestCloudSystemFinder(
    AbstractCloudStatusWatcher* watcher, QObject* parent):
    CloudSystemFinder(watcher, parent)
{
}

void TestCloudSystemFinder::simulatePingResponse(
    const QString& cloudSystemId,
    bool success,
    const nx::vms::api::ModuleInformationWithAddresses& moduleInfo)
{
    nx::network::rest::JsonResult jsonResult;
    jsonResult.setReply(moduleInfo);
    const auto messageBody = QJson::serialized(jsonResult);

    onRequestComplete(cloudSystemId, success, messageBody);
}

} // namespace nx::vms::client::core::test
