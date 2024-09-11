// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/common/system_health/message_type.h>

namespace nx::vms::event { class StringsHelper; }

namespace nx::vms::client::desktop {

/** Helper class that simplifies control over right panel notification settings. */
class SiteNotificationSettingsManager: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    explicit SiteNotificationSettingsManager(SystemContext* systemContext, QWidget* parent);

    /** Returns event types supported by the system. */
    QList<api::EventType> supportedEventTypes() const;

    /**
     * Returns event types watched by the current user or std::nullopt if there is no current user.
     */
    std::optional<QList<api::EventType>> watchedEvents() const;

    /**
     * Set watched event types for the current user. If there is no current user, nothing
     * has been changed.
     */
    void setWatchedEvents(const QList<api::EventType>& events);

    /** Returns message types supported by the system. */
    std::set<common::system_health::MessageType> supportedMessageTypes() const;

    /** Returns message types watched by the system. */
    std::set<common::system_health::MessageType> watchedMessages() const;

    /** Set watched message types. */
    void setWatchedMessages(const std::set<common::system_health::MessageType>& messages);

signals:
    /** Emits whether watched event or message types are changed. */
    void settingsChanged();

private:
    QWidget* m_parent;
    core::UserResourcePtr m_currentUser;

    void onCurrentUserChanged(const QnUserResourcePtr& user);
};

} // namespace nx::vms::client::desktop
