// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <nx/vms/common/system_health/message_type.h>

#include "../call_notifications_list_model.h"

namespace nx::vms::client::desktop {

class CallNotificationsListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(CallNotificationsListModel* q);
    virtual ~Private() override;

private:
    void addNotification(
        nx::vms::common::system_health::MessageType message,
        const QVariant& params);
    void removeNotification(
        nx::vms::common::system_health::MessageType message,
        const QVariant& params);

    QString tooltip(const vms::event::AbstractActionPtr& action) const;

private:
    CallNotificationsListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
};

} // namespace nx::vms::client::desktop
