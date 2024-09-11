// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "site_notification_settings_manager.h"

#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

SiteNotificationSettingsManager::SiteNotificationSettingsManager(
    SystemContext* systemContext,
    QWidget* parent)
    :
    QObject{parent},
    SystemContextAware(systemContext),
    m_parent{parent}
{
    connect(
        systemContext,
        &SystemContext::userChanged,
        this,
        &SiteNotificationSettingsManager::onCurrentUserChanged);

    connect(
        &appContext()->localSettings()->popupSystemHealth,
        &utils::property_storage::BaseProperty::changed,
        this,
        &SiteNotificationSettingsManager::settingsChanged);

    onCurrentUserChanged(systemContext->user());
}

QList<api::EventType> SiteNotificationSettingsManager::supportedEventTypes() const
{
    return event::allEvents({
        event::isNonDeprecatedEvent,
        event::isApplicableForLicensingMode(systemContext())});
}

std::optional<QList<api::EventType>> SiteNotificationSettingsManager::watchedEvents() const
{
    if (m_currentUser)
        return m_currentUser->settings().watchedEvents();

    return std::nullopt;
}

void SiteNotificationSettingsManager::setWatchedEvents(const QList<api::EventType>& events)
{
    if (!m_currentUser)
        return;

    auto userSettings = m_currentUser->settings();
    if (events != watchedEvents())
    {
        // TODO: #mmalofeev remove sessionTokenHelper when https://networkoptix.atlassian.net/browse/VMS-52731 will be fixed.
        auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
            m_parent,
            tr("Save user"),
            tr("Enter your account password"),
            tr("Save"),
            FreshSessionTokenHelper::ActionType::updateSettings);

        userSettings.setWatchedEvents(events);
        // settingsChanged signal will be emitted automatically if stored events changed.
        m_currentUser->saveSettings(userSettings, sessionTokenHelper);
    }
}

std::set<common::system_health::MessageType> SiteNotificationSettingsManager::supportedMessageTypes() const
{
    return common::system_health::allMessageTypes({
        common::system_health::isMessageVisibleInSettings,
        common::system_health::isMessageApplicableForLicensingMode(systemContext())});
}

std::set<common::system_health::MessageType> SiteNotificationSettingsManager::watchedMessages() const
{
    return appContext()->localSettings()->popupSystemHealth();
}

void SiteNotificationSettingsManager::setWatchedMessages(
    const std::set<common::system_health::MessageType>& messages)
{
    // settingsChanged signal will be emitted automatically if stored messages changed.
    appContext()->localSettings()->popupSystemHealth = messages;
}

void SiteNotificationSettingsManager::onCurrentUserChanged(const QnUserResourcePtr& user)
{
    if (m_currentUser == user)
        return;

    if (m_currentUser)
        m_currentUser->disconnect(this);

    m_currentUser = user.dynamicCast<nx::vms::client::core::UserResource>();

    if (!m_currentUser)
        return;

    connect(
        m_currentUser.get(),
        &QnUserResource::userSettingsChanged,
        this,
        &SiteNotificationSettingsManager::settingsChanged);

    emit settingsChanged();
}

} // namespace nx::vms::client::desktop
