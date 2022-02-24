// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <QtGui/QColor>

#include <qt_graphics_items/graphics_widget.h>

class QnResourceWidget;

namespace nx::vms::client::desktop {
namespace ui {

class SelectionWidget: public GraphicsWidget
{
public:
    SelectionWidget(QnResourceWidget* parent);

    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    QColor calculateFrameColor() const;

    void paintSelection(QPainter* painter);

    /**
    * Inner frame is painted when there is no layout cell spacing.
    */
    void paintInnerFrame(QPainter* painter);

    /**
    * Inner frame is painted when there is not-empty layout cell spacing.
    */
    void paintOuterFrame(QPainter* painter);

private:
    QPointer<QnResourceWidget> m_widget;
};

} // namespace ui
} // namespace nx::vms::client::desktop
