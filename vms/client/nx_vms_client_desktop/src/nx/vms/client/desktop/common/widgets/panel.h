// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class Panel: public QWidget
{
    Q_OBJECT

public:
    explicit Panel(QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
