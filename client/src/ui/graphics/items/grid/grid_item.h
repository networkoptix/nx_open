#ifndef QN_GRID_ITEM_H
#define QN_GRID_ITEM_H

#include <QtCore/QWeakPointer>
#include <QtGui/QGraphicsObject>

#include <utils/common/hash.h> /* For qHash(QPoint). */

#include <ui/animation/animated.h>

class AnimationTimer;

class QnWorkbenchGridMapper;
class VariantAnimator;
class QnGridHighlightItem;

class QnGridItem: public Animated<QGraphicsObject> {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)

    typedef Animated<QGraphicsObject> base_type;

public:
    enum CellState {
        INITIAL,
        ALLOWED,
        DISALLOWED,
        CUSTOM
    };

    QnGridItem(QGraphicsItem *parent = NULL);
    
    virtual ~QnGridItem();

    virtual QRectF boundingRect() const override;

    QnWorkbenchGridMapper *mapper() const {
        return m_mapper.data();
    }

    void setMapper(QnWorkbenchGridMapper *mapper);

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color) {
        m_color = color;
    }

    qreal lineWidth() const {
        return m_lineWidth;
    }

    void setLineWidth(qreal lineWidth) {
        m_lineWidth = lineWidth;
    }

    QColor stateColor(int cellState) const;

    void setStateColor(int cellState, const QColor &color);

    int cellState(const QPoint &cell) const;

    void setCellState(const QPoint &cell, int cellState);

    void setCellState(const QSet<QPoint> &cells, int cellState);

    void setCellState(const QRect &cells, int cellState);

    void setCellState(const QList<QRect> &cells, int cellState);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QnGridHighlightItem *newHighlightItem();

    QPoint itemCell(QnGridHighlightItem *item) const;
    void setItemCell(QnGridHighlightItem *item, const QPoint &cell) const;
    VariantAnimator *itemAnimator(QnGridHighlightItem *item);

protected slots:
    void at_itemAnimator_finished();

private:
    struct PointData {
        PointData(): state(INITIAL), item(NULL) {}

        int state;
        QnGridHighlightItem *item;
    };

    QRectF m_boundingRect;
    QWeakPointer<QnWorkbenchGridMapper> m_mapper;
    QColor m_color;
    qreal m_lineWidth;
    QHash<int, QColor> m_colorByState;
    QHash<QPoint, PointData> m_dataByCell;
    QList<QnGridHighlightItem *> m_freeItems;
};


#endif // QN_GRID_ITEM_H
