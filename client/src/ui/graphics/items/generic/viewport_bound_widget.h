#pragma once

#include <ui/graphics/items/standard/graphics_widget.h>

class QGraphicsScale;
class QGraphicsView;

class Instrument;

/**
 * Graphics widget with a coordinate system that has the same scale as the
 * viewport, regardless of its size.
 * 
 * Items added to this widget will not be scaled when viewport's transformation 
 * changes.
 */
class QnViewportBoundWidget: public GraphicsWidget {
    Q_OBJECT

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

    virtual void setGeometry(const QRectF &geometry) override;

signals:
    void scaleUpdated(QGraphicsView *view, qreal scale);

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

public slots:
    void updateScale(QGraphicsView *view = NULL);

private:
    QSizeF m_fixedSize;
    bool m_inUpdateScale;
    QPointer<QGraphicsView> m_lastView;
    QPointer<Instrument> m_instrument;
    QGraphicsScale *m_scale;
};
