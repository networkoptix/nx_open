#include "email_manager_mocked.h"

//-------------------------------------------------------------------------------------------------
// TestEmailManager

void TestEmailManager::sendAsync(
    const nx::cloud::db::AbstractNotification& notification,
    std::function<void(bool)> completionHandler)
{
    if (m_onNotificationHandler)
        m_onNotificationHandler(notification);
    if (completionHandler)
        completionHandler(true);
}

void TestEmailManager::setOnReceivedNotification(OnNotificationHandler handler)
{
    m_onNotificationHandler = std::move(handler);
}

//-------------------------------------------------------------------------------------------------
// EmailManagerMocked

void EmailManagerMocked::sendAsync(
    const nx::cloud::db::AbstractNotification& notification,
    std::function<void(bool)> completionHandler)
{
    sendAsyncMocked(notification);
    BaseType::sendAsync(notification, std::move(completionHandler));
}
