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

    QnUuid addItem(QGraphicsWidget *item, const QnUuid &externalId = QnUuid());
    QnUuid insertItem(int index, QGraphicsWidget *item, const QnUuid &externalId = QnUuid());

    void removeItem(const QnUuid &id);
    void clear();

    int overlayWidth() const;
    void setOverlayWidth(int width);

    QSizeF minimumSize() const;

    QSizeF maxFillCoeff() const;
    void setMaxFillCoeff(const QSizeF &coeff);

private:
    void updatePositions();

private:
    QGraphicsWidget * const m_contentWidget;
    QnGraphicsScrollArea * const m_scrollArea;
    QGraphicsLinearLayout * const m_mainLayout;

    struct ItemData
    {
        QnUuid id;
        QGraphicsWidget* item;
        ItemData();
        ItemData(const QnUuid& id, QGraphicsWidget* item);
    };

    /* Order is more important than fast lookup. */
    QList<ItemData> m_items;
    Qt::Alignment m_alignment;

    bool m_updating;

    QSizeF m_maxFillCoeff;
};
