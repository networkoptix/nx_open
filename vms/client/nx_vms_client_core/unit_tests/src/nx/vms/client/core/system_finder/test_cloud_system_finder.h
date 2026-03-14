// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/system_finder/private/cloud_system_finder.h>

namespace nx::vms::client::core {

class AbstractCloudStatusWatcher;

namespace test {

/**
 * Allows testing the ping response handling logic without actual network calls.
 */
class TestCloudSystemFinder: public CloudSystemFinder
{
    using base_type = CloudSystemFinder;

public:
    TestCloudSystemFinder(AbstractCloudStatusWatcher* watcher, QObject* parent = nullptr);

    /**
     * Simulate receiving a ping response from a cloud system.
     */
    void simulatePingResponse(
        const QString& cloudSystemId,
        bool success,
        const nx::vms::api::ModuleInformationWithAddresses& moduleInfo);
};

} // namespace test

} // namespace nx::vms::client::core
