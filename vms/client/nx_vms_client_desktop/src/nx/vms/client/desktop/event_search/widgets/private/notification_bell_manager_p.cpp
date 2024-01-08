// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_bell_manager_p.h"

#include <nx/vms/event/actions/abstract_action.h>

#include "notification_bell_widget_p.h"

namespace nx::vms::client::desktop {

NotificationBellManager::NotificationBellManager(
    NotificationBellWidget* widget,
    QObject* parent)
    :
    QObject(parent),
    m_widget(widget)
{
}

NotificationBellManager::~NotificationBellManager()
{
}

void NotificationBellManager::setAlarmStateActive(bool active)
{
    m_isActive = active;

    if (!active)
        m_widget->setAlarmState(false);
}

void NotificationBellManager::handleNotificationAdded(
    nx::vms::common::system_health::MessageType message,
    const QVariant& params)
{
    countNotification(message, params, true);
}

void NotificationBellManager::handleNotificationRemoved(
    nx::vms::common::system_health::MessageType message,
    const QVariant& params)
{
    countNotification(message, params, false);
}

void NotificationBellManager::countNotification(
    nx::vms::common::system_health::MessageType message,
    const QVariant& params,
    bool add)
{
    if (message != nx::vms::common::system_health::MessageType::showIntercomInformer)
        return;

    if (!params.canConvert<vms::event::AbstractActionPtr>())
        return;

    auto action = params.value<vms::event::AbstractActionPtr>();
    const auto& runtimeParameters = action->getRuntimeParams();
    const QnUuid intercomId = runtimeParameters.eventResourceId;

    if (add)
        m_activeNotifications.insert(intercomId);
    else
        m_activeNotifications.erase(intercomId);

    if (!m_isActive)
        return;

    m_widget->setAlarmState(!m_activeNotifications.empty());
}

} // nx::vms::client::desktop
