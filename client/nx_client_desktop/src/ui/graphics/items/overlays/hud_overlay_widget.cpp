#include "hud_overlay_widget.h"
#include "private/hud_overlay_widget_p.h"

#include <ui/graphics/items/generic/viewport_bound_widget.h>

QnHudOverlayWidget::QnHudOverlayWidget(QGraphicsItem* parent):
    base_type(parent),
    d_ptr(new QnHudOverlayWidgetPrivate(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(QGraphicsItem::ItemClipsChildrenToShape);
}

QnHudOverlayWidget::~QnHudOverlayWidget()
{
}

QnResourceTitleItem* QnHudOverlayWidget::title() const
{
    Q_D(const QnHudOverlayWidget);
    return d->title;
}

QnViewportBoundWidget* QnHudOverlayWidget::content() const
{
    Q_D(const QnHudOverlayWidget);
    return d->content;
}

QnHtmlTextItem* QnHudOverlayWidget::details() const
{
    Q_D(const QnHudOverlayWidget);
    return d->details;
}

QnHtmlTextItem* QnHudOverlayWidget::position() const
{
    Q_D(const QnHudOverlayWidget);
    return d->position;
}

QGraphicsWidget* QnHudOverlayWidget::left() const
{
    Q_D(const QnHudOverlayWidget);
    return d->left;
}

QGraphicsWidget* QnHudOverlayWidget::right() const
{
    Q_D(const QnHudOverlayWidget);
    return d->right;
}
