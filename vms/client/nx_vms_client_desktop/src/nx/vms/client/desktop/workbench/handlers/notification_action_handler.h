// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::api {

enum class EventReason;
struct SiteHealthMessage;

} // namespace nx::vms::api

namespace nx::vms::client::desktop {

class NotificationActionHandler:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    NotificationActionHandler(WindowContext* windowContext, QObject* parent = nullptr);
    virtual ~NotificationActionHandler() override;

    void removeNotification(const nx::vms::api::SiteHealthMessage& message);

signals:
    void systemHealthEventAdded(const nx::vms::api::SiteHealthMessage& message);
    void systemHealthEventRemoved(const nx::vms::api::SiteHealthMessage& message);

    void cleared();

private:
    void clear();

    void at_context_userChanged();
    void onSystemHealthMessage(const nx::vms::api::SiteHealthMessage& message);
    void at_serviceDisabled(
        nx::vms::api::EventReason reason, const std::set<nx::Uuid>& deviceIds);
    void at_saasDataChanged();
};

} // namespace nx::vms::client::desktop
