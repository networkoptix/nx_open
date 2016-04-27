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
    QnUuid addItem(QGraphicsWidget *item, const QnUuid &externalId = QnUuid());
    QnUuid insertItem(int index, QGraphicsWidget *item, const QnUuid &externalId = QnUuid());

    void removeItem(const QnUuid &id);
    void clear();

    int overlayWidth() const;
    void setOverlayWidth(int width);

    /**
     * Maximum size of the parent widget that can be filled with this overlay.
     * Counted in the fractions of the whole, e.g. fill coeff (0.5, 0.8) means
     * that the overlay will take only half of the parent widget by width and
     * 0.8 of the parent widget by height (on maximum).
     */
    QSizeF maxFillCoeff() const;
    void setMaxFillCoeff(const QSizeF &coeff);

    QSizeF contentSize() const;

signals:
    void contentSizeChanged();

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

private:
    Q_DECLARE_PRIVATE(QnScrollableOverlayWidget)
    QScopedPointer<QnScrollableOverlayWidgetPrivate> d_ptr;
};
