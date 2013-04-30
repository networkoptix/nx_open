#ifndef QN_CONSTRAINED_GEOMETRICALLY_H
#define QN_CONSTRAINED_GEOMETRICALLY_H

#include <QtCore/QRectF>

#include <common/common_globals.h>

class ConstrainedGeometrically {
public:
    virtual ~ConstrainedGeometrically() {}

    virtual QRectF constrainedGeometry(const QRectF &geometry, Qn::Corner pinCorner, const QPointF &pinPoint = QPointF()) const = 0;
};


#endif // QN_CONSTRAINED_GEOMETRICALLY_H
