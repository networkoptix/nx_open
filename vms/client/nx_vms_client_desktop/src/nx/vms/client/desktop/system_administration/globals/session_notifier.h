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

    virtual bool tryClose(bool /*force*/) override { emit closeRequested(); return true; }

signals:
    void closeRequested();
};

} // namespace nx::vms::client::desktop
