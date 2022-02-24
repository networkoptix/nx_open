// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

class QnCommonMessageProcessor;
class QnResourcePool;

namespace nx::vms::client::desktop {

class AnalyticsSettingsManager;

/**
 * Initializes Analytics Settings Manager and creates actual implementations of all it's external
 * interfaces (listening to the message bus and sending requests over server rest api).
 */
class AnalyticsSettingsManagerFactory
{
public:
    static std::unique_ptr<AnalyticsSettingsManager> createAnalyticsSettingsManager(
        QnResourcePool* resourcePool,
        QnCommonMessageProcessor* messageProcessor);
};

} // namespace nx::vms::client::desktop
