#pragma once

#include <ui/graphics/items/generic/viewport_bound_widget.h>

#include <utils/common/uuid.h>

class QnScrollableOverlayWidgetPrivate;

/**
 *  Container overlay class, that displays items on the scrollable area.
 */
class QnScrollableOverlayWidget : public QnViewportBoundWidget {
    Q_OBJECT

    typedef QnViewportBoundWidget base_type;
public:
    QnScrollableOverlayWidget(Qt::Alignment alignment, QGraphicsWidget *parent = nullptr);
    virtual ~QnScrollableOverlayWidget();

    /** Add item to scrollable area. Will take ownership of the item. */
    QnUuid addItem(QGraphicsWidget *item);
    void removeItem(const QnUuid &id);
    void clear();

    int overlayWidth() const;
    void setOverlayWidth(int width);

private:
    Q_DECLARE_PRIVATE(QnScrollableOverlayWidget)
    QScopedPointer<QnScrollableOverlayWidgetPrivate> d_ptr;
};
