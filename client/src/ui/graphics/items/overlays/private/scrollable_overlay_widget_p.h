#pragma once

#include <ui/graphics/items/generic/graphics_scroll_area.h>
#include <ui/graphics/items/overlays/scrollable_overlay_widget.h>

#include <utils/common/uuid.h>

class QnScrollableOverlayWidgetPrivate
{
    Q_DECLARE_PUBLIC(QnScrollableOverlayWidget)
    QnScrollableOverlayWidget *q_ptr;

public:
    QnScrollableOverlayWidgetPrivate(Qt::Alignment alignment, QnScrollableOverlayWidget *parent);
    virtual ~QnScrollableOverlayWidgetPrivate();

    QnUuid addItem(QGraphicsWidget *item);
    void removeItem(const QnUuid &id);
    void clear();

    int overlayWidth() const;
    void setOverlayWidth(int width);

private:
    void updatePositions();

private:
    QGraphicsWidget * const m_contentWidget;
    QnGraphicsScrollArea * const m_scrollArea;
    QGraphicsLinearLayout * const m_mainLayout;

    QHash<QnUuid, QGraphicsWidget*> m_items;
    Qt::Alignment m_alignment;
    bool m_updating;
};
