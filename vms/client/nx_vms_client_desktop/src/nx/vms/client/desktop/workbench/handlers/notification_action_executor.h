// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/rules/acknowledge.h>
#include <nx/vms/rules/action_executor.h>
#include <nx/vms/rules/actions/actions_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

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

    void setAcknowledge(nx::vms::api::rules::EventLogRecordList&& records);

signals:
    void notificationActionReceived(
        const QSharedPointer<nx::vms::rules::NotificationActionBase>& notificationAction,
        const QString& cloudSystemId);

private:
    virtual void execute(const nx::vms::rules::ActionPtr& action) override;

    void onNotificationActionReceived(
        const QSharedPointer<nx::vms::rules::NotificationActionBase>& notificationAction,
        const QString& cloudSystemId);
    void reinitializeCrossSystemNotificationsListener();
    void handleAcknowledgeAction();

    void removeNotification(const nx::vms::rules::NotificationActionBasePtr& action);

private:
    std::unique_ptr<CrossSystemNotificationsListener> m_listener;
};

} // namespace nx::vms::client::desktop
