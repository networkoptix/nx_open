// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_session_token_expiration_watcher.h"

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/text/time_strings.h>

namespace nx::vms::client::desktop {

LocalSessionTokenExpirationWatcher::LocalSessionTokenExpirationWatcher(
    QPointer<workbench::LocalNotificationsManager> notificationManager,
    QObject* parent)
    :
    QObject(parent),
    m_notificationManager(notificationManager)
{
}

void LocalSessionTokenExpirationWatcher::setTimeoutWatcher(
    core::RemoteSessionTimeoutWatcher* sessionTimeoutWatcher)
{
    if (m_sessionTimeoutWatcher)
        m_sessionTimeoutWatcher->disconnect(this);

    m_sessionTimeoutWatcher = sessionTimeoutWatcher;

    if (!m_sessionTimeoutWatcher)
        return;

    connect(
        sessionTimeoutWatcher,
        &nx::vms::client::core::RemoteSessionTimeoutWatcher::showNotification,
        this,
        [this](std::chrono::minutes timeLeft)
        {
            if (m_notification)
                setNotificationTimeLeft(timeLeft);
            else
                notify(timeLeft);
        });

    connect(
        sessionTimeoutWatcher,
        &nx::vms::client::core::RemoteSessionTimeoutWatcher::hideNotification,
        this,
        [this]()
        {
            NX_ASSERT(m_notification);

            m_notificationManager->remove(*m_notification);
            m_notification = std::nullopt;
        });
}

void LocalSessionTokenExpirationWatcher::notify(std::chrono::minutes timeLeft)
{
    NX_VERBOSE(this, "Notify session expires: %1", timeLeft);

    m_notification =
        m_notificationManager->add(
            tr("Your session expires soon"), "", /*cancellable*/ true);

    m_notificationManager->setLevel(*m_notification, nx::vms::event::Level::important);

    setNotificationTimeLeft(timeLeft);

    auto action = CommandActionPtr(new CommandAction(tr("Re-Authenticate Now")));
    connect(
        action.get(),
        &CommandAction::triggered,
        this,
        &LocalSessionTokenExpirationWatcher::authenticationRequested);

    m_notificationManager->setAction(*m_notification, action);

    connect(
        m_notificationManager,
        &workbench::LocalNotificationsManager::cancelRequested,
        this,
        [this](nx::Uuid notificationId)
        {
            if (notificationId == m_notification)
                m_sessionTimeoutWatcher->notificationHiddenByUser();
        });

    connect(
        m_notificationManager,
        &workbench::LocalNotificationsManager::interactionRequested,
        this,
        [this, action](nx::Uuid notificationId)
        {
            if (notificationId == m_notification)
                action->trigger();
        });
}

void LocalSessionTokenExpirationWatcher::setNotificationTimeLeft(std::chrono::minutes timeLeft)
{
    const auto minutesLeftStr = (timeLeft.count() == 0)
        ? tr("Less than a minute left")
        : tr("%n minutes left", "", timeLeft.count());
    m_notificationManager->setDescription(*m_notification, minutesLeftStr);
}

} // namespace nx::vms::client::desktop
