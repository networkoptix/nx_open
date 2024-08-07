// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QAbstractButton>

namespace nx::vms::client::desktop {

class CloseTabButton: public QAbstractButton
{
    Q_OBJECT
    using base_type = QAbstractButton;

public:
    explicit CloseTabButton(QWidget* parent = 0);
    virtual void paintEvent(QPaintEvent* event) override;
};

} // namespace nx::vms::client::desktop
