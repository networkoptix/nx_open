#include "test_email_manager.h"

namespace nx::cloud::db {

TestEmailManager::TestEmailManager(
    nx::utils::MoveOnlyFunc<void(const AbstractNotification&)> _delegate)
    :
    m_delegate(std::move(_delegate))
{
}

void TestEmailManager::sendAsync(
    const AbstractNotification& notification,
    std::function<void(bool)> completionHandler)
{
    if (m_delegate)
        m_delegate(notification);

    if (completionHandler)
        completionHandler(true);
}

} // namespace nx::cloud::db
