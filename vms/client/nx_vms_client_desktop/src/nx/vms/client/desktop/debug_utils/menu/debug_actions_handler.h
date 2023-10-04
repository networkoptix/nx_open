// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class DebugActionsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit DebugActionsHandler(QObject* parent = nullptr);
    virtual ~DebugActionsHandler() override;

private:
    void registerDebugCounterActions();

    void enableSecurityForPowerUsers(bool value);

    void at_debugIncrementCounterAction_triggered();
    void at_debugDecrementCounterAction_triggered();
};

} // namespace nx::vms::client::desktop
