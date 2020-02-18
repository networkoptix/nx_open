#pragma once

#include <memory>

class QnDesktopClientMessageProcessor;
class QnResourcePool;

namespace nx::vms::client::desktop {

class AnalyticsSettingsManager;

/**
 * Initializes Analytics Settings Manager and creates actual implementations of all it's external
 * interfaces (listening to
 */
class AnalyticsSettingsManagerFactory
{
public:
    static std::unique_ptr<AnalyticsSettingsManager> createAnalyticsSettingsManager(
        QnResourcePool* resourcePool,
        QnDesktopClientMessageProcessor* messageProcessor);
};

} // namespace nx::vms::client::desktop
