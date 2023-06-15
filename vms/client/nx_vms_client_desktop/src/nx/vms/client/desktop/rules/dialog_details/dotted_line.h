// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPainter>
#include <QtWidgets/QWidget>

#include <nx/vms/client/core/skin/color_theme.h>

namespace nx::vms::client::desktop::rules {

class DottedLineWidget: public QWidget
{
    Q_OBJECT

public:
    explicit DottedLineWidget(QWidget* parent = nullptr): QWidget{parent}
    {
        setFixedHeight(1);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setPen(QPen(core::colorTheme()->color("dark11"), 2, Qt::DotLine));
        painter.drawLine(0, 0, width(), 0);
    }
};

} // namespace nx::vms::client::desktop::rules
