#pragma once

#include <QtCore/QPointer>

#include <QtGui/QColor>

#include <ui/graphics/items/standard/graphics_widget.h>

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
