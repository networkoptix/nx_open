// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/common/system_health/message_type.h>

namespace nx::vms::client::desktop {

/** Helper class that simplifies control over right panel notification settings. */
class UserNotificationSettingsManager: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    explicit UserNotificationSettingsManager(
        SystemContext* systemContext, QObject* parent = nullptr);

    /** List of all events that may appear in settings. */
    QList<api::EventType> allEvents() const;

    /** Returns event types supported by the system. */
    const QList<api::EventType>& supportedEventTypes() const;

    /**
     * Returns event types watched by the current user or empty list if there is no current user.
     */
    const QList<api::EventType>& watchedEvents() const;

    /** List of all events that may appear in settings. */
    const QList<common::system_health::MessageType> allMessages() const;

    /** Returns message types supported by the system. */
    const QList<common::system_health::MessageType>& supportedMessageTypes() const;

    /** Returns message types watched by the current user or empty list if there is no current user. */
    const QList<common::system_health::MessageType>& watchedMessages() const;

    /**
     * Set watched event and message types for the current user. If there is no current user,
     * nothing has been changed.
     */
    void setSettings(
        const QList<api::EventType>& events,
        const QList<common::system_health::MessageType>& messages);

signals:
    /** Emits whether supported event ot message types are changed. */
    void supportedTypesChanged();

    /** Emits whether watched event or message types are changed. */
    void settingsChanged();

private:
    core::UserResourcePtr m_currentUser;
    QList<api::EventType> m_supportedEventTypes;
    QList<api::EventType> m_watchedEventTypes;
    QList<common::system_health::MessageType> m_supportedMessageTypes;
    QList<common::system_health::MessageType> m_watchedMessageTypes;

    void onCurrentUserChanged(const QnUserResourcePtr& user);
    void onLicensingModeChanged();

    void updateSupportedTypes();
    void updateWatchedTypes();
};

} // namespace nx::vms::client::desktop
