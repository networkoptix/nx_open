// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/workbench/workbench_state_manager.h>

namespace nx::vms::client::desktop {

class SessionNotifier: public QObject, public QnSessionAwareDelegate
{
    Q_OBJECT

    using base_type = QnSessionAwareDelegate;

public:
    SessionNotifier(QObject* parent): QObject(parent), base_type(parent) {};

    ui::action::Manager* actionManager() const { return menu(); }

    virtual bool tryClose(bool /*force*/) override { emit closeRequested(); return true; }
    virtual void forcedUpdate() override { emit forcedUpdateRequested(); }

signals:
    void closeRequested();
    void forcedUpdateRequested();
};

} // namespace nx::vms::client::desktop
