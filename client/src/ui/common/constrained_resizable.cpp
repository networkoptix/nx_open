#include "constrained_resizable.h"

QRectF ConstrainedResizable::constrainedGeometry(const QRectF &geometry, Qt::WindowFrameSection pinSection, const QPointF &pinPoint, const QSizeF &constrainedSize)
{
    if(qFuzzyEquals(constrainedSize, geometry.size()))
        return geometry;

    if(pinSection == Qt::NoSection)
        return QnGeometry::scaled(geometry, constrainedSize, pinPoint, Qt::IgnoreAspectRatio);

    QPointF pinSectionPoint = Qn::calculatePinPoint(geometry, pinSection);
    QRectF result(
        pinSectionPoint - QnGeometry::cwiseMul(QnGeometry::cwiseDiv(pinSectionPoint - geometry.topLeft(), geometry.size()), constrainedSize),
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
