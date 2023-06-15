// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPainter>
#include <QtWidgets/QWidget>

#include <nx/vms/client/core/skin/color_theme.h>

namespace nx::vms::client::desktop::rules {

class StyledFrame: public QWidget
{
    Q_OBJECT

public:
    explicit StyledFrame(QWidget* parent = nullptr): QWidget{parent}
    {
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(core::colorTheme()->color("dark8"));
        painter.setPen(QPen(core::colorTheme()->color("dark13"), 1));
        painter.drawRoundedRect(rect(), 4, 4);
    }
};

} // namespace nx::vms::client::desktop::rules
