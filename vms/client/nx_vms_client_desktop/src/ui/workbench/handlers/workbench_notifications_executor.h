// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_executor.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::rules { class NotificationAction; }

class QnWorkbenchNotificationsExecutor:
    public nx::vms::rules::ActionExecutor,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    QnWorkbenchNotificationsExecutor(QObject* parent);

signals:
    void notificationActionReceived(
        const QSharedPointer<nx::vms::rules::NotificationAction>& notificationAction);

private:
    virtual void execute(const nx::vms::rules::ActionPtr& action) override;
};
