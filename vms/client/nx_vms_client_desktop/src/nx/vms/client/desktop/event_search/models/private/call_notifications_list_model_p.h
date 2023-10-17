// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "../call_notifications_list_model.h"

namespace nx::vms::client::desktop {

class CallNotificationsListModel::Private:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(CallNotificationsListModel* q);
    virtual ~Private() override;

private:
    void addNotification(const vms::event::AbstractActionPtr& action);
    void removeNotification(const vms::event::AbstractActionPtr& action);

    QString tooltip(const vms::event::AbstractActionPtr& action) const;

private:
    CallNotificationsListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
};

} // namespace nx::vms::client::desktop
