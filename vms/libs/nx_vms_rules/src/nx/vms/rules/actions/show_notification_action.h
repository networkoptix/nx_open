// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/client_action.h>
#include <nx/vms/rules/icon.h>

#include "notification_action_base.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API NotificationAction: public NotificationActionBase
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.desktopNotification")

    // Data fields.
    FIELD(std::chrono::microseconds, interval, setInterval)
    FIELD(bool, acknowledge, setAcknowledge)

    // Notification look and feel fields.
    FIELD(nx::vms::rules::Icon, icon, setIcon)
    FIELD(QString, customIcon, setCustomIcon)
    FIELD(nx::vms::rules::ClientAction, clientAction, setClientAction)
    FIELD(QString, url, setUrl)
    FIELD(QString, extendedCaption, setExtendedCaption);

public:
    NotificationAction() = default;
    static const ItemDescriptor& manifest();

    virtual QVariantMap details(common::SystemContext* context) const override;
};

} // namespace nx::vms::rules
