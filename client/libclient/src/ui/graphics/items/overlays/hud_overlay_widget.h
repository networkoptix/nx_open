#pragma once

#include <client/client_color_types.h>

#include <ui/customization/customized.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>

class QnResourceTitleItem;
class QnHtmlTextItem;
struct QnResourceHudColors;

class QnHudOverlayWidgetPrivate;

class QnHudOverlayWidget: public Customized<QnViewportBoundWidget>
{
    Q_OBJECT
    Q_PROPERTY(QnResourceHudColors colors READ colors WRITE setColors)

    using base_type = Customized<QnViewportBoundWidget>;

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

    /** Colors. */
    QnResourceHudColors colors() const;
    void setColors(const QnResourceHudColors& colors);

private:
    Q_DECLARE_PRIVATE(QnHudOverlayWidget)
    const QScopedPointer<QnHudOverlayWidgetPrivate> d_ptr;
};
