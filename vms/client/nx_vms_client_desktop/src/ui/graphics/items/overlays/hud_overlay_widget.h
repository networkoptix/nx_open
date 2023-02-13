// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <qt_graphics_items/graphics_widget.h>

#include <nx/vms/client/desktop/window_context_aware.h>

class QnResourceTitleItem;
class QnViewportBoundWidget;
class QnHtmlTextItem;
class QnImageButtonBar;
class QnHudOverlayWidgetPrivate;

namespace nx::vms::client::desktop {
class PlaybackPositionItem;
class WindowContext;
} // namespace nx::vms::client::desktop

class QnHudOverlayWidget: public GraphicsWidget, nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    QnHudOverlayWidget(
        nx::vms::client::desktop::WindowContext* windowContext,
        QGraphicsItem* parent = nullptr);
    virtual ~QnHudOverlayWidget();

    /** Resource title bar item. */
    QnResourceTitleItem* title() const;

    /** Resource bottom bar item. */
    nx::vms::client::desktop::PlaybackPositionItem* playbackPositionItem() const;

    /** Everything under title bar. */
    QnViewportBoundWidget* content() const;

    /** Resource details text item. */
    QnHtmlTextItem* details() const;

    /** Left container for additional data (above details). */
    QGraphicsWidget* left() const;

    /** Right container for additional data (above position). */
    QGraphicsWidget* right() const;

    /** Current action indicator text. When empty, action indication is hidden. */
    QString actionText() const;
    void setActionText(const QString& value);
    void clearActionText(std::chrono::milliseconds after);

private:
    Q_DECLARE_PRIVATE(QnHudOverlayWidget)
    const QScopedPointer<QnHudOverlayWidgetPrivate> d_ptr;
};
