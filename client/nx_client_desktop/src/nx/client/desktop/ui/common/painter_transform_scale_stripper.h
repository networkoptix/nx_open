#pragma once

#include <QtGui/QPainter>
#include <QtGui/QTransform>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class PainterTransformScaleStripper
{
public:
    PainterTransformScaleStripper(QPainter* painter);
    ~PainterTransformScaleStripper();

    QTransform::TransformationType type() const;
    QRectF mapRect(const QRectF& rect) const;
    QPointF mapPoint(const QPointF& point) const;

    const QTransform& transform() const;

private:
    QPainter* m_painter;
    QTransform m_originalTransform;
    QTransform::TransformationType m_type;
    QTransform m_transform;
    bool m_alignable = false;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
