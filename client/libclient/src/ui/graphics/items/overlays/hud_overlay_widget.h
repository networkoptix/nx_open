#pragma once

#include <ui/graphics/items/generic/viewport_bound_widget.h>

class QnResourceTitleItem;
class QnHtmlTextItem;

class QnHudOverlayWidgetPrivate;

class QnHudOverlayWidget: public QnViewportBoundWidget
{
    Q_OBJECT
    using base_type = QnViewportBoundWidget;

public:
    QnHudOverlayWidget(QGraphicsItem* parent = nullptr);
    virtual ~QnHudOverlayWidget();

    /** Resource title bar item. */
    QnResourceTitleItem* title() const;

    /** Resource details text item. */
    QnHtmlTextItem* details() const;

    /** Resource position text item. */
    QnHtmlTextItem* position() const;

    /** Left container for additional data (above details). */
    QGraphicsWidget* left() const;

    /** Right container for additional data (above position). */
    QGraphicsWidget* right() const;

private:
    Q_DECLARE_PRIVATE(QnHudOverlayWidget)
    const QScopedPointer<QnHudOverlayWidgetPrivate> d_ptr;
};
