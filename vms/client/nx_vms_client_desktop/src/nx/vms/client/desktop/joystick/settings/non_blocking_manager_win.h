// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "manager.h"

namespace nx::vms::client::desktop::joystick {

class NonBlockingManagerWin: public Manager
{
    Q_OBJECT

    using base_type = Manager;

public:
    NonBlockingManagerWin(QObject* parent = 0);
    virtual ~NonBlockingManagerWin() override = default;

public slots:
    void onDeviceListChanged();
};

} // namespace nx::vms::client::desktop::joystick
