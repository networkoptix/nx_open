// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hud_overlay_widget.h"
#include "private/hud_overlay_widget_p.h"

#include <ui/common/palette.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>

#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;

QnHudOverlayWidget::QnHudOverlayWidget(QGraphicsItem* parent):
    base_type(parent),
    d_ptr(new QnHudOverlayWidgetPrivate(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
    setPaletteColor(this, QPalette::Window, colorTheme()->color("camera.hudElementsBackground"));
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

QString QnHudOverlayWidget::actionText() const
{
    Q_D(const QnHudOverlayWidget);
    return d->actionIndicator->text();
}

void QnHudOverlayWidget::setActionText(const QString& value)
{
    Q_D(QnHudOverlayWidget);
    d->actionIndicator->setText(value);
}

void QnHudOverlayWidget::clearActionText(std::chrono::milliseconds after)
{
    Q_D(QnHudOverlayWidget);
    d->actionIndicator->clear(after);
}
