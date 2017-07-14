#pragma once

#include <ui/customization/customized.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnResourceTitleItem;
class QnViewportBoundWidget;
class QnHtmlTextItem;
struct QnResourceHudColors;

class QnHudOverlayWidgetPrivate;

class QnHudOverlayWidget: public Customized<GraphicsWidget>
{
    Q_OBJECT
    using base_type = Customized<GraphicsWidget>;

public:
    QnHudOverlayWidget(QGraphicsItem* parent = nullptr);
    virtual ~QnHudOverlayWidget();

    /** Resource title bar item. */
    QnResourceTitleItem* title() const;

    /** Everything under title bar. */
    QnViewportBoundWidget* content() const;

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
