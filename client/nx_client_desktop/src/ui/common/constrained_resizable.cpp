#include "constrained_resizable.h"

#include <nx/client/core/utils/geometry.h>

QRectF ConstrainedResizable::constrainedGeometry(const QRectF &geometry, Qt::WindowFrameSection pinSection, const QPointF &pinPoint, const QSizeF &constrainedSize)
{
    using nx::client::core::Geometry;

    if(qFuzzyEquals(constrainedSize, geometry.size()))
        return geometry;

    if(pinSection == Qt::NoSection || pinSection == Qt::TitleBarArea)
        return Geometry::scaled(geometry, constrainedSize, pinPoint, Qt::IgnoreAspectRatio);

    QPointF pinSectionPoint = Qn::calculatePinPoint(geometry, pinSection);
    QRectF result(
        pinSectionPoint - Geometry::cwiseMul(
            Geometry::cwiseDiv(pinSectionPoint - geometry.topLeft(), geometry.size()),
            constrainedSize),
        constrainedSize
        );

    /* Handle zero size so that we don't return NaNs. */
    if(qFuzzyIsNull(geometry.width())) {
        if(pinSection == Qt::TopLeftSection || pinSection == Qt::BottomLeftSection || pinSection == Qt::LeftSection) {
            result.moveLeft(geometry.left());
        } else {
            result.moveLeft(geometry.left() - result.width());
        }
    }
    if(qFuzzyIsNull(geometry.height())) {
        if(pinSection == Qt::TopLeftSection || pinSection == Qt::TopRightSection || pinSection == Qt::TopSection) {
            result.moveTop(geometry.top());
        } else {
            result.moveTop(geometry.top() - result.height());
        }
    }

    return result;
}

ConstrainedResizable::~ConstrainedResizable()
{

}
