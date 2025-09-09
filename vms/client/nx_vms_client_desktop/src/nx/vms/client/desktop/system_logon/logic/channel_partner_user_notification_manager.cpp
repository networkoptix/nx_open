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
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/event/level.h>

namespace nx::vms::client::desktop {

bool userIsChannelPartner(const QnUserResourcePtr& user)
{
    return user->attributes().testFlag(nx::vms::api::UserAttribute::channelPartner);
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
    if (!checkBasicConstantVisibilityConditions())
        return;

    if (checkBasicMutableVisibilityConditions() && systemContainsChannelPartnerUser(system()))
    {
        showNotification();
    }
    else
    {
        m_connections << connect(
            system()->resourcePool(), &QnResourcePool::resourcesAdded,
            this, &ChannelPartnerUserNotificationManager::handleUserAdded);

        m_connections << connect(
            system()->accessController(),
            &nx::vms::client::core::AccessController::userChanged,
            this,
            &ChannelPartnerUserNotificationManager::handleUserChanged);

        m_connections << connect(
            system()->accessController(),
            &nx::vms::client::core::AccessController::globalPermissionsChanged,
            this,
            &ChannelPartnerUserNotificationManager::handleUserChanged);
    }

    m_connections << connect(
        system()->saasServiceManager(),
        &nx::vms::common::saas::ServiceManager::dataChanged,
        this,
        [this]
        {
            if (m_notificationId.isNull())
                return;

            if (!nx::vms::common::saas::saasInitialized(system()))
            {
                // Most probably, the System has been disconnected from the Cloud.
                workbench()->windowContext()->localNotificationsManager()->remove(m_notificationId);
                m_notificationId = {};
            }
        });
}

void ChannelPartnerUserNotificationManager::handleDisconnect()
{
    m_connections.reset();

    if (m_notificationId.isNull())
        return;

    workbench()->windowContext()->localNotificationsManager()->remove(m_notificationId);
    m_notificationId = {};
}

void ChannelPartnerUserNotificationManager::handleUserAdded(const QnResourceList& resources)
{
    if (!checkBasicConstantVisibilityConditions() || !checkBasicMutableVisibilityConditions())
        return;

    for (const auto& user: resources.filtered<nx::vms::client::core::UserResource>())
    {
        if (user && userIsChannelPartner(user))
        {
            showNotification();
            return;
        }
    }
}

void ChannelPartnerUserNotificationManager::handleUserChanged()
{
    if (!checkBasicConstantVisibilityConditions() || !checkBasicMutableVisibilityConditions())
        return;

    if (systemContainsChannelPartnerUser(system()))
        showNotification();
}

bool ChannelPartnerUserNotificationManager::checkBasicConstantVisibilityConditions() const
{
    if (system()->localSettings()->channelPartnerUserNotificationClosed())
        return false;

    if (!m_notificationId.isNull())
        return false;

    return true;
}

bool ChannelPartnerUserNotificationManager::checkBasicMutableVisibilityConditions() const
{
    const bool isChannelPartnerUser = userIsChannelPartner(system()->user());
    const auto isPowerUser = system()->accessController()->hasPowerUserPermissions();
    return !isChannelPartnerUser && isPowerUser;
}

void ChannelPartnerUserNotificationManager::showNotification()
{
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
    notificationsManager->setLevel(m_notificationId, nx::vms::event::Level::important);
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

} // namespace nx::vms::client::desktop
