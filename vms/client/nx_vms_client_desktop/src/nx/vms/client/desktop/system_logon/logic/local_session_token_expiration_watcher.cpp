// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_session_token_expiration_watcher.h"

#include <QtGui/QAction>

#include <client_core/client_core_module.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/text/time_strings.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

LocalSessionTokenExpirationWatcher::LocalSessionTokenExpirationWatcher(
    QPointer<workbench::LocalNotificationsManager> notificationManager,
    QObject* parent)
    :
    m_notificationManager(notificationManager)
{
    auto sessionTimeoutWatcher = qnClientCoreModule->networkModule()->sessionTimeoutWatcher();

    connect(
        sessionTimeoutWatcher,
        &nx::vms::client::core::RemoteSessionTimeoutWatcher::showNotification,
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
        [this]()
        {
            NX_ASSERT(m_notification);

            m_notificationManager->remove(*m_notification);
            m_notification = std::nullopt;
        });
}

void LocalSessionTokenExpirationWatcher::notify(std::chrono::minutes timeLeft)
{
    m_notification =
        m_notificationManager->add(
            tr("Your session expires soon"), "", /*cancellable*/ true);

    m_notificationManager->setLevel(
        *m_notification, QnNotificationLevel::Value::ImportantNotification);

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
        [this](nx::Uuid notificationId)
        {
            if (notificationId == m_notification)
            {
                qnClientCoreModule->networkModule()
                    ->sessionTimeoutWatcher()->notificationHiddenByUser();
            }
        });

    connect(
        m_notificationManager,
        &workbench::LocalNotificationsManager::interactionRequested,
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
