// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_widget.h>

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
    using base_type = GraphicsWidget;

public:
    QnViewportBoundWidget(QGraphicsItem* parent = nullptr);
    virtual ~QnViewportBoundWidget();

    /**
     * \returns                         Fixed size of this graphics widget, in parent coordinates.
     */
    const QSizeF& fixedSize();

    /**
     * \param desiredSize               Fixed size of this graphics widget, in parent coordinates.
     */
    void setFixedSize(const QSizeF& fixedSize);

    virtual void setGeometry(const QRectF& geometry) override;

    void updateScale();
    virtual void updateGeometry() override;

    qreal scale() const; //< Absolute scale
    qreal sceneScale() const;
    qreal relativeScale() const; //< scale() / sceneScale()

signals:
    void scaleChanged();

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QSizeF m_fixedSize;
    QGraphicsScale* m_scale;
    QnViewportScaleWatcher* m_scaleWatcher;
    bool m_inUpdateScale = false;
};
