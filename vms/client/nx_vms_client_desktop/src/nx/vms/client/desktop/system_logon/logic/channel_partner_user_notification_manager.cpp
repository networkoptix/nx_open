// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "channel_partner_user_notification_manager.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/notification_levels.h>

namespace nx::vms::client::desktop {

bool userIsChannelPartner(const QnUserResourcePtr& user)
{
    return user && user->attributes().testFlag(nx::vms::api::UserAttribute::hidden);
}

bool userIsHidden(const QnUserResourcePtr& user)
{
    return user && user->attributes().testFlag(nx::vms::api::UserAttribute::hidden);
}

bool systemContainsHiddenUsers(nx::vms::client::desktop::SystemContext* system)
{
    return system->resourcePool()->contains<nx::vms::client::core::UserResource>(
        [](const nx::vms::client::core::UserResourcePtr& user)
        {
            return userIsHidden(user);
        });
}

ChannelPartnerUserNotificationManager::ChannelPartnerUserNotificationManager(
    WindowContext* windowContext,
    QObject* parent)
    :
    QObject(parent),
    WindowContextAware(windowContext)
{
}

void ChannelPartnerUserNotificationManager::handleConnect()
{
    // Handle user list changes.
    m_connections << connect(
        system()->resourcePool(), &QnResourcePool::resourcesAdded,
        this, &ChannelPartnerUserNotificationManager::handleUserAdded);
    m_connections << connect(
        system()->resourcePool(), &QnResourcePool::resourcesRemoved,
        this, &ChannelPartnerUserNotificationManager::handleUserRemoved);

    // Handle current User permission changes.
    m_connections << connect(
        system()->accessController(),
        &nx::vms::client::core::AccessController::userChanged,
        this,
        &ChannelPartnerUserNotificationManager::updateNotificationState);
    m_connections << connect(
        system()->accessController(),
        &nx::vms::client::core::AccessController::globalPermissionsChanged,
        this,
        &ChannelPartnerUserNotificationManager::updateNotificationState);

    // Handle user interactions with the Notification.
    const auto notificationsManager = workbench()->windowContext()->localNotificationsManager();
    m_connections << connect(
        notificationsManager, &workbench::LocalNotificationsManager::linkActivated,
        [this](const nx::Uuid& notificationId, const QString& linkUrl)
        {
            if (notificationId != m_notificationId)
                return;

            HelpHandler::openHelpTopic(HelpTopic::Id::ChannelPartnerUser);
        });
    m_connections << connect(
        notificationsManager, &workbench::LocalNotificationsManager::cancelRequested,
        [this, localSettings = system()->localSettings()](const nx::Uuid& notificationId)
        {
            if (notificationId != m_notificationId)
                return;

            localSettings->channelPartnerUserNotificationClosed = true;
            // Notification state is updated below.
        });

    // Handle settings changes. Also shows the Notification if User clicks 'Reset All Warnings'.
    m_connections << connect(system()->localSettings(), &SystemSpecificLocalSettings::changed,
        [this, localSettings = system()->localSettings()](
            const utils::property_storage::BaseProperty* property)
        {
            if (!property)
                return;

            if (property->name == localSettings->channelPartnerUserNotificationClosed.name)
                updateNotificationState();
        });

    // A minor preliminary optimization: usage of PendingOperation prevents O(n^2) accesses
    // to user list attributes in the case of batch user processing.
    m_checkUsersOperation.reset(new nx::utils::PendingOperation(
        [this]
        {
            m_hasHiddenUsers = systemContainsHiddenUsers(system());
            updateNotificationState();
        },
        100));

    // Process existing users and update the notification state.
    handleUserAdded(system()->resourcePool()->getResources<nx::vms::client::core::UserResource>());
}

void ChannelPartnerUserNotificationManager::handleDisconnect()
{
    m_connections.reset();
    m_hasHiddenUsers = false;
    updateNotificationState();
}

void ChannelPartnerUserNotificationManager::handleUserAdded(const QnResourceList& resources)
{
    for (const auto& user: resources.filtered<nx::vms::client::core::UserResource>())
    {
        m_hasHiddenUsers |= userIsHidden(user);

        // We could omit those users who don't have a `channelPartner` attribute, but it's possible
        // theoretically that someone will change the user type on the fly (e.g., org <-> CP).
        // Therefore, it's safer to watch over all cloud users.
        if (user && user->userType() == api::UserType::cloud)
        {
            connect(
                user.get(), &QnUserResource::attributesChanged,
                m_checkUsersOperation.get(), &nx::utils::PendingOperation::requestOperation);
        }
    }

    updateNotificationState();
}

void ChannelPartnerUserNotificationManager::handleUserRemoved(const QnResourceList& resources)
{
    bool update = false;
    for (const auto& user: resources.filtered<nx::vms::client::core::UserResource>())
    {
        update |= userIsHidden(user);

        // Match the condition with the one in handleUserAdded().
        if (user && user->userType() == api::UserType::cloud)
            user->disconnect(this);
    }

    if (update)
    {
        m_hasHiddenUsers = systemContainsHiddenUsers(system());
        updateNotificationState();
    }
}

bool ChannelPartnerUserNotificationManager::notificationHasBeenClosed() const
{
    return system()->localSettings()->channelPartnerUserNotificationClosed();
}

bool ChannelPartnerUserNotificationManager::userCanSeeNotification() const
{
    const bool isChannelPartnerUser = userIsChannelPartner(system()->user());
    const auto isPowerUser = system()->accessController()->hasPowerUserPermissions();
    return !isChannelPartnerUser && isPowerUser;
}

bool ChannelPartnerUserNotificationManager::systemHasHiddenUsers() const
{
    return m_hasHiddenUsers;
}

void ChannelPartnerUserNotificationManager::updateNotificationState()
{
    const bool show =
        userCanSeeNotification() && !notificationHasBeenClosed()
            && systemHasHiddenUsers();

    if (show)
    {
        if (!m_notificationId.isNull())
            return;

        QStringList messageLines;
        messageLines << tr("Channel Partner users' access is managed at the Organization level, "
                "and they are not visible in site user management.");
        messageLines << QString();
        messageLines << nx::vms::common::html::localLink(tr("Learn more"));

        const auto notificationsManager = workbench()->windowContext()->localNotificationsManager();
        m_notificationId = notificationsManager->add(
            tr("Channel Partner users have access to this site"),
            messageLines.join(nx::vms::common::html::kLineBreak),
            /*cancellable*/ true);
        notificationsManager->setIconPath(
            m_notificationId, "20x20/Outline/warning.svg");
        notificationsManager->setLevel(
            m_notificationId, QnNotificationLevel::Value::ImportantNotification);
    }
    else
    {
        if (m_notificationId.isNull())
            return;

        workbench()->windowContext()->localNotificationsManager()->remove(m_notificationId);
        m_notificationId = {};
    }
}

} // namespace nx::vms::client::desktop
