#pragma once

#include <ui/graphics/items/standard/graphics_widget.h>

class QGraphicsScale;

class QnViewportScaleWatcher;

/**
 * Graphics widget with a coordinate system that has the same scale as the
 * viewport, regardless of its size.
 *
 * Items added to this widget will not be scaled when viewport's transformation
 * changes.
 */
class QnViewportBoundWidget: public GraphicsWidget
{
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

    void updateScale();
protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QSizeF m_fixedSize;

    QGraphicsScale *m_scale;
    QnViewportScaleWatcher* m_scaleWatcher;
};
