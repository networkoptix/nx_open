#ifndef QN_VIEWPORT_BOUND_WIDGET_H
#define QN_VIEWPORT_BOUND_WIDGET_H

#include <QGraphicsWidget>

class Instrument;

/**
 * Graphics widget with a coordinate system that has the same scale as the
 * viewport, regardless of its size.
 * 
 * Items added to this widget will not be scaled when viewport's transformation 
 * changes.
 */
class QnViewportBoundWidget: public QGraphicsWidget {
    Q_OBJECT;

    typedef QGraphicsWidget base_type;

public:
    QnViewportBoundWidget(QGraphicsItem *parent = NULL);
    virtual ~QnViewportBoundWidget();

    /**
     * \returns                         Desired size of this graphics widget, in parent coordinates. 
     */
    const QSizeF &desiredSize();

    /**
     * \param desiredSize               Desired size of this graphics widget, in parent coordinates. 
     */
    void setDesiredSize(const QSizeF &desiredSize);

protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

protected slots:
    void updateScale(QGraphicsView *view = NULL);

private:
    QSizeF m_desiredSize;
    bool m_inUpdateScale;
    QWeakPointer<QGraphicsView> m_lastView;
    QWeakPointer<Instrument> m_instrument;
};


#endif // QN_VIEWPORT_BOUND_WIDGET_H
