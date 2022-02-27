// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_GRID_ITEM_H
#define QN_GRID_ITEM_H

#include <QtCore/QPointer>
#include <QtWidgets/QGraphicsObject>

#include <utils/common/hash.h> /* For qHash(QPoint). */
#include <ui/utils/viewport_scale_watcher.h>
#include <ui/animation/animated.h>

class AnimationTimer;

class QnWorkbenchGridMapper;
class VariantAnimator;
class QnGridHighlightItem;

class QnGridItem: public Animated<QGraphicsObject>
{
    Q_OBJECT

    typedef Animated<QGraphicsObject> base_type;

public:
    enum CellState {
        Initial,
        Allowed,
        Disallowed,
        Custom
    };

    QnGridItem(QGraphicsItem *parent = nullptr);

    virtual ~QnGridItem();

    virtual QRectF boundingRect() const override;

    QnWorkbenchGridMapper *mapper() const;
    void setMapper(QnWorkbenchGridMapper *mapper);

    QColor stateColor(int cellState) const;

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
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    struct PointData {
        PointData(): state(Initial), item(nullptr) {}

        int state;
        QnGridHighlightItem *item;
    };

    QRectF m_boundingRect;
    QPointer<QnWorkbenchGridMapper> m_mapper;
    qreal m_lineWidth = 0.0;
    QHash<int, QColor> m_colorByState;
    QHash<QPoint, PointData> m_dataByCell;
    QList<QnGridHighlightItem *> m_freeItems;
    QnViewportScaleWatcher m_scaleWatcher;
};


#endif // QN_GRID_ITEM_H
