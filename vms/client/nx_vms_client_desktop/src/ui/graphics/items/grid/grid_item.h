#ifndef QN_GRID_ITEM_H
#define QN_GRID_ITEM_H

#include <QtCore/QPointer>
#include <QtWidgets/QGraphicsObject>

#include <client/client_color_types.h>

#include <utils/common/hash.h> /* For qHash(QPoint). */
#include <ui/utils/viewport_scale_watcher.h>
#include <ui/animation/animated.h>
#include <ui/customization/customized.h>

class AnimationTimer;

class QnWorkbenchGridMapper;
class VariantAnimator;
class QnGridHighlightItem;

class QnGridItem: public Customized<Animated<QGraphicsObject> > {
    Q_OBJECT
    Q_PROPERTY(QnGridColors colors READ colors WRITE setColors)

    typedef Customized<Animated<QGraphicsObject> > base_type;

public:
    enum CellState {
        Initial,
        Allowed,
        Disallowed,
        Custom
    };

    QnGridItem(QGraphicsItem *parent = NULL);

    virtual ~QnGridItem();

    virtual QRectF boundingRect() const override;

    QnWorkbenchGridMapper *mapper() const;
    void setMapper(QnWorkbenchGridMapper *mapper);

    const QnGridColors &colors() const;
    void setColors(const QnGridColors &colors);

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
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    struct PointData {
        PointData(): state(Initial), item(NULL) {}

        int state;
        QnGridHighlightItem *item;
    };

    QRectF m_boundingRect;
    QPointer<QnWorkbenchGridMapper> m_mapper;
    QnGridColors m_colors;
    qreal m_lineWidth;
    QHash<int, QColor> m_colorByState;
    QHash<QPoint, PointData> m_dataByCell;
    QList<QnGridHighlightItem *> m_freeItems;
    QnViewportScaleWatcher m_scaleWatcher;
};


#endif // QN_GRID_ITEM_H
