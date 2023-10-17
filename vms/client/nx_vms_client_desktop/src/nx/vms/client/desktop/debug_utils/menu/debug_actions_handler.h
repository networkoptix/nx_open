// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

class DebugActionsHandler: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    DebugActionsHandler(WindowContext* windowContext, QObject* parent = nullptr);
    virtual ~DebugActionsHandler() override;

private:
    void registerDebugCounterActions();

    void enableSecurityForPowerUsers(bool value);

    void at_debugIncrementCounterAction_triggered();
    void at_debugDecrementCounterAction_triggered();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
