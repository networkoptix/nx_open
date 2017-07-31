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

private:
    QPainter* m_painter;
    QTransform m_originalTransform;
    QTransform::TransformationType m_type;
    QTransform m_transform;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
