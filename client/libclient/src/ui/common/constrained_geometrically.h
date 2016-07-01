#ifndef QN_CONSTRAINED_GEOMETRICALLY_H
#define QN_CONSTRAINED_GEOMETRICALLY_H

#include <QtCore/QRectF>

#include <common/common_globals.h>

/**
 * This class is a workaround for a lack of mechanism that would allow to constrain
 * the widget's geometry when dragging and/or resizing.
 */
class ConstrainedGeometrically {
public:
    virtual ~ConstrainedGeometrically() {}

    /**
     * \param geometry                  Desired new geometry.
     * \param pinSection                Section that is currently pinned, if any.
     * \param pinPoint                  If there is no pinned section,
     *                                  then parent coordinates of the pinned point are passed here.
     * \returns                         New geometry for the widget that satisfies the constraints.
     */
    virtual QRectF constrainedGeometry(const QRectF &geometry, Qt::WindowFrameSection pinSection, const QPointF &pinPoint = QPointF()) const = 0;
};


#endif // QN_CONSTRAINED_GEOMETRICALLY_H
