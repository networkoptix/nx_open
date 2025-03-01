// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::client::desktop {

class NotificationActionHandler:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    using MessageType = nx::vms::common::system_health::MessageType;

    NotificationActionHandler(WindowContext* windowContext, QObject* parent = nullptr);
    virtual ~NotificationActionHandler() override;

    void removeNotification(const vms::event::AbstractActionPtr& action);

signals:
    void systemHealthEventAdded(
        MessageType message, const nx::vms::event::AbstractActionPtr& action);
    void systemHealthEventRemoved(
        MessageType message, const nx::vms::event::AbstractActionPtr& action);

    void cleared();

private:
    void clear();

    void at_context_userChanged();
    void onSystemHealthMessage(const nx::vms::event::AbstractActionPtr& action);
    void at_serviceDisabled(
        nx::vms::api::EventReason reason, const std::set<nx::Uuid>& deviceIds);

    void setSystemHealthEventVisible(
        const nx::vms::event::AbstractActionPtr& action, bool visible);
};

} // namespace nx::vms::client::desktop
