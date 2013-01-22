#ifndef QN_VIEWPORT_BOUND_WIDGET_H
#define QN_VIEWPORT_BOUND_WIDGET_H

#include <ui/graphics/items/standard/graphics_widget.h>

class QGraphicsScale;

class Instrument;

/**
 * Graphics widget with a coordinate system that has the same scale as the
 * viewport, regardless of its size.
 * 
 * Items added to this widget will not be scaled when viewport's transformation 
 * changes.
 */
class QnViewportBoundWidget: public GraphicsWidget {
    Q_OBJECT;

    typedef GraphicsWidget base_type;

public:
    QnViewportBoundWidget(QGraphicsItem *parent = NULL);
    virtual ~QnViewportBoundWidget();

    /**
     * \returns                         Fixed size of this graphics widget, in parent coordinates. 
     */
    const QSizeF &fixedSize();

    /**
     * \param desiredSize               Fixed size of this graphics widget, in parent coordinates. 
     */
    void setFixedSize(const QSizeF &fixedSize);

protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

protected slots:
    void updateScale(QGraphicsView *view = NULL);

private:
    QSizeF m_fixedSize;
    bool m_inUpdateScale;
    QWeakPointer<QGraphicsView> m_lastView;
    QWeakPointer<Instrument> m_instrument;
    QGraphicsScale *m_scale;
};


#endif // QN_VIEWPORT_BOUND_WIDGET_H
