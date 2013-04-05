#ifndef QN_CONSTRAINED_GEOMETRICALLY_H
#define QN_CONSTRAINED_GEOMETRICALLY_H

#include <QtCore/QRectF>

class ConstrainedGeometrically {
public:
    virtual ~ConstrainedGeometrically() {}

    virtual QRectF constrainedGeometry(const QRectF &geometry, const QPointF *pinPoint) const = 0;
};


#endif // QN_CONSTRAINED_GEOMETRICALLY_H
