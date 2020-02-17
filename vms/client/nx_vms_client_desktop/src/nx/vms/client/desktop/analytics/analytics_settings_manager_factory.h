#pragma once

#include <memory>

class QnDesktopClientMessageProcessor;

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
        QnDesktopClientMessageProcessor* messageProcessor);
};

} // namespace nx::vms::client::desktop
