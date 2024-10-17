// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_notification_settings_manager.h"

#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/migration_utils.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

UserNotificationSettingsManager::UserNotificationSettingsManager(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject{parent},
    SystemContextAware(systemContext)
{
    connect(
        systemContext,
        &SystemContext::userChanged,
        this,
        &UserNotificationSettingsManager::onCurrentUserChanged);

    connect(
        systemContext->saasServiceManager(),
        &common::saas::ServiceManager::saasStateChanged,
        this,
        &UserNotificationSettingsManager::onLicensingModeChanged);

    updateSupportedTypes();
    onCurrentUserChanged(systemContext->user());
}

const QList<api::EventType>& UserNotificationSettingsManager::supportedEventTypes() const
{
    return m_supportedEventTypes;
}

const QList<api::EventType>& UserNotificationSettingsManager::watchedEvents() const
{
    return m_watchedEventTypes;
}

const QList<common::system_health::MessageType>& UserNotificationSettingsManager::supportedMessageTypes() const
{
    return m_supportedMessageTypes;
}

const QList<common::system_health::MessageType>& UserNotificationSettingsManager::watchedMessages() const
{
    return m_watchedMessageTypes;
}

void UserNotificationSettingsManager::setSettings(
    const QList<api::EventType>& events,
    const QList<common::system_health::MessageType>& messages)
{
    if (!m_currentUser || (messages == watchedMessages() && events == watchedEvents()))
        return;

    auto userSettings = m_currentUser->settings();

    {
        std::set<std::string> banListOfEvents;

        // Extract values from the types user was able to edit.
        for (auto e: m_supportedEventTypes)
        {
            if (!events.contains(e))
                banListOfEvents.insert(event::convertToNewEvent(e).toStdString());
        }

        // Unsupported at the moment types must not be lost.
        for (const auto& e: userSettings.eventFilter)
        {
            if (!m_supportedEventTypes.contains(event::convertToOldEvent(QString::fromStdString(e))))
                banListOfEvents.insert(e);
        }

        userSettings.eventFilter = std::move(banListOfEvents);
    }

    {
        std::set<std::string> banListOfMessages;

        // Extract values from the types user was able to edit.
        for (auto m: m_supportedMessageTypes)
        {
            if (!messages.contains(m))
                banListOfMessages.insert(reflect::toString(m));
        }

        // Unsupported at the moment types must not be lost.
        for (const auto& m: userSettings.messageFilter)
        {
            bool isOk{};
            auto messageType = reflect::fromString<common::system_health::MessageType>(m, &isOk);
            if (!isOk)
                continue; //< Enum value was removed, no sense to support it any more.

            if (!m_supportedMessageTypes.contains(messageType))
                banListOfMessages.insert(m);
        }

        userSettings.messageFilter = std::move(banListOfMessages);
    }

    // TODO: #mmalofeev remove sessionTokenHelper when https://networkoptix.atlassian.net/browse/VMS-52731 will be fixed.
    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        appContext()->mainWindowContext()->mainWindowWidget(),
        tr("Save user"),
        tr("Enter your account password"),
        tr("Save"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    // settingsChanged signal will be emitted automatically if stored events/messages changed.
    m_currentUser->saveSettings(userSettings, sessionTokenHelper);
}

void UserNotificationSettingsManager::onCurrentUserChanged(const QnUserResourcePtr& user)
{
    if (m_currentUser == user)
        return;

    if (m_currentUser)
        m_currentUser->disconnect(this);

    m_currentUser = user.dynamicCast<nx::vms::client::core::UserResource>();

    updateWatchedTypes();

    if (!m_currentUser)
        return;

    connect(
        m_currentUser.get(),
        &QnUserResource::userSettingsChanged,
        this,
        [this]
        {
            updateWatchedTypes();

            emit settingsChanged();
        });

    emit settingsChanged();
}

void UserNotificationSettingsManager::onLicensingModeChanged()
{
    updateSupportedTypes();
    updateWatchedTypes();

    emit supportedTypesChanged();
    emit settingsChanged();
}

void UserNotificationSettingsManager::updateSupportedTypes()
{
    m_supportedMessageTypes = common::system_health::visibleInSettingsMessages(systemContext());
    m_supportedEventTypes = event::visibleInSettingsEvents(systemContext());
}

void UserNotificationSettingsManager::updateWatchedTypes()
{
    if (!m_currentUser)
    {
        m_watchedEventTypes.clear();
        m_watchedMessageTypes.clear();

        return;
    }

    const auto userSettings = m_currentUser->settings();

    m_watchedEventTypes = m_supportedEventTypes;
    for (const auto& blockedEventType: userSettings.eventFilter)
    {
        m_watchedEventTypes.removeOne(
            event::convertToOldEvent(QString::fromStdString(blockedEventType)));
    }

    m_watchedMessageTypes = m_supportedMessageTypes;
    for (const auto& blockedMessageType: userSettings.messageFilter)
    {
        bool isOk{};
        auto messageType =
            reflect::fromString<common::system_health::MessageType>(blockedMessageType, &isOk);

        if (!isOk)
            continue;

        m_watchedMessageTypes.removeOne(messageType);
    }
}

} // namespace nx::vms::client::desktop
