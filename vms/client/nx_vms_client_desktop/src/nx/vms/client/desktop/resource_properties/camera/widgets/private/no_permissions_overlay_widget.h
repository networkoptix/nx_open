// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class NoPermissionsOverlayWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit NoPermissionsOverlayWidget(QWidget* parent = nullptr);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
};

} // namespace nx::vms::client::desktop
