// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_executor.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::rules { class NotificationActionBase; }

namespace nx::vms::client::desktop {

class CrossSystemNotificationsListener;

class NotificationActionExecutor:
    public nx::vms::rules::ActionExecutor,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit NotificationActionExecutor(QObject* parent);
    ~NotificationActionExecutor();

signals:
    void notificationActionReceived(
        const QSharedPointer<nx::vms::rules::NotificationActionBase>& notificationAction,
        const QString& cloudSystemId);

private:
    virtual void execute(const nx::vms::rules::ActionPtr& action) override;

    void onContextUserChanged();

private:
    std::unique_ptr<CrossSystemNotificationsListener> m_listener;
};

} // namespace nx::vms::client::desktop
