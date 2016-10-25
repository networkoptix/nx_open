#include "email_manager_mocked.h"

void EmailManagerMocked::sendAsync(
    const nx::cdb::AbstractNotification& notification,
    std::function<void(bool)> completionHandler)
{
    const auto serializedNotification = notification.serializeToJson();
    sendAsyncMocked(notification);
    if (completionHandler)
        completionHandler(true);
}
