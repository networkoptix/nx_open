// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class CloudActionsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    CloudActionsHandler(QObject *parent = nullptr);
    virtual ~CloudActionsHandler() override;

private:
    void at_loginToCloudAction_triggered();
    void at_logoutFromCloudAction_triggered();

    void at_forcedLogout();
};

} // namespace nx::vms::client::desktop
