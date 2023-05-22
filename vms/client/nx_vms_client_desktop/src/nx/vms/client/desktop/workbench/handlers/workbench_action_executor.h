// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_executor.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::rules { class NotificationActionBase; }

namespace nx::vms::client::desktop {

class WorkbenchActionExecutor:
    public nx::vms::rules::ActionExecutor,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit WorkbenchActionExecutor(QObject* parent);
    ~WorkbenchActionExecutor();

private:
    virtual void execute(const nx::vms::rules::ActionPtr& action) override;
};

} // namespace nx::vms::client::desktop
