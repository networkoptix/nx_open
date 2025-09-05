// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "channel_partner_user_notification_manager.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/common/notification_levels.h>

namespace nx::vms::client::desktop {

bool userIsChannelPartner(const QnUserResourcePtr& user)
{
    return user->attributes().testFlag(nx::vms::api::UserAttribute::hidden);
}

bool systemContainsChannelPartnerUser(nx::vms::client::desktop::SystemContext* system)
{
    return system->resourcePool()->contains<nx::vms::client::core::UserResource>(
        [](const nx::vms::client::core::UserResourcePtr& user)
        {
            return userIsChannelPartner(user);
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
    if (system()->localSettings()->channelPartnerUserNotificationClosed())
        return;

    const bool isChannelPartnerUser = userIsChannelPartner(system()->user());
    const auto isPowerUser = system()->accessController()->hasPowerUserPermissions();
    if (isChannelPartnerUser || !isPowerUser)
        return;

    if (!systemContainsChannelPartnerUser(system()))
        return;

    const auto notificationsManager = workbench()->windowContext()->localNotificationsManager();
    m_notificationId = notificationsManager->add(
        tr("Channel Partner users have access to this site"),
        tr("Channel Partner users' access is managed at the Organization level, "
            "and they are not visible in site user management."
            "<br/><br/><a href=\"%1\">Learn more</a>")
        .arg(HelpTopic::relativeUrlForTopic(HelpTopic::Id::ChannelPartnerUser)),
        /*cancellable*/ true);
    notificationsManager->setIconPath(
        m_notificationId, "20x20/Outline/warning.svg");
    notificationsManager->setLevel(
        m_notificationId, QnNotificationLevel::Value::ImportantNotification);
    QObject::connect(notificationsManager, &workbench::LocalNotificationsManager::cancelRequested,
        [this,
        localSettings = system()->localSettings(),
        notificationsManager](
            const nx::Uuid& notificationId)
        {
            if (notificationId != m_notificationId)
                return;

            notificationsManager->remove(m_notificationId);
            m_notificationId = {};
            localSettings->channelPartnerUserNotificationClosed = true;
        });
}

void ChannelPartnerUserNotificationManager::handleDisconnect()
{
    if (m_notificationId.isNull())
        return;

    workbench()->windowContext()->localNotificationsManager()->remove(m_notificationId);
    m_notificationId = {};
}

} // namespace nx::vms::client::desktop
