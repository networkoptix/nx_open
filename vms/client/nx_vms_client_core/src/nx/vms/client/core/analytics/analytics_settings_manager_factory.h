// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

namespace nx::vms::client::core {

class AnalyticsSettingsManager;
class SystemContext;

/**
 * Initializes Analytics Settings Manager and creates actual implementations of all it's external
 * interfaces (listening to the message bus and sending requests over server rest api).
 */
class NX_VMS_CLIENT_CORE_API AnalyticsSettingsManagerFactory
{
public:
    // TODO: #sivanov Move all this logic to the manager itself.
    static std::unique_ptr<AnalyticsSettingsManager> createAnalyticsSettingsManager(
        SystemContext* systemContext);
};

} // namespace nx::vms::client::core
