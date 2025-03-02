// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_notification_settings_manager.h"

#include <ranges>

#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/compatibility.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::common::system_health;
using namespace nx::vms::rules;

namespace {

using namespace nx::vms::rules::utils;

static const auto kAllEventFilters = ItemFilters{isItemActual, isItemExternal};
static const auto kVisibleEventFilters = ItemFilters(kAllEventFilters) << isItemSupportedLicense;

QStringList filterEvents(SystemContext* context, const ItemFilters& filters)
{
    return nx::utils::toQList(
        filterItems(context, filters, context->vmsRulesEngine()->events().values())
            | std::views::transform([](const auto& desc) { return desc.id; }));
}

} // namespace

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

QStringList UserNotificationSettingsManager::allEvents() const
{
    return filterEvents(systemContext(), kAllEventFilters);
}

const QStringList& UserNotificationSettingsManager::supportedEventTypes() const
{
    return m_supportedEventTypes;
}

const QStringList& UserNotificationSettingsManager::watchedEvents() const
{
    return m_watchedEventTypes;
}

const QList<MessageType> UserNotificationSettingsManager::allMessages() const
{
    return nx::utils::toQList(allMessageTypes({isMessageVisibleInSettings}));
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
    const QStringList& events,
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
                banListOfEvents.insert(e.toStdString());
        }

        // Unsupported at the moment types must not be lost.
        for (const auto& e: userSettings.eventFilter)
        {
            if (!m_supportedEventTypes.contains(QString::fromStdString(e)))
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

    // Sync settings for all other sites, so cross-site layouts will show correct set of
    // notifications.
    if (m_currentUser->isCloud())
        appContext()->cloudStatusWatcher()->saveUserSettings(userSettings);

    // settingsChanged signal will be emitted automatically if stored events/messages changed.
    m_currentUser->saveSettings(userSettings);
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
    m_supportedMessageTypes = nx::utils::toQList(allMessageTypes(
        {isMessageVisibleInSettings, isMessageApplicableForLicensingMode(systemContext())}));

    m_supportedEventTypes = filterEvents(systemContext(), kVisibleEventFilters);
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
        m_watchedEventTypes.removeOne(QString::fromStdString(blockedEventType));
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
