#ifndef QN_CONSTRAINED_RESIZABLE
#define QN_CONSTRAINED_RESIZABLE

#include <utils/math/fuzzy.h>

#include "constrained_geometrically.h"
#include "geometry.h"
#include "frame_section.h"

/**
 * This class is a workaround for a lack of mechanisms that would allow to 
 * create widgets with constant aspect ratio. 
 * 
 * Default sizeHint / sizePolicy system cannot help here because when passing 
 * constraint to effectiveSizeHint function, either width or height must not be set. 
 * If both are set, then you get your constraint back and the control never 
 * reaches the virtual sizeHint function. 
 * 
 * That is, there is no simple way to implement constant aspect ratio 
 * resizing using Qt-supplied functionality. So we introduce a separate 
 * interface for that.
 */
class ConstrainedResizable: public ConstrainedGeometrically {
public:
    /**
     * Virtual destructor.
     */
    virtual ~ConstrainedResizable();

    /**
     * This function is to be called just before the derived class's geometry
     * is to be changed.
     *
     * Given a size constraint, it returns a preferred size that satisfies it. 
     * The result of this function must not exceed \a constraint, but it need 
     * not be equal to it.
     * 
     * Note that it is user's responsibility to make sure that minimal size hint
     * satisfies the constraint.
     *
     * \param constraint                Constraint for the result. Must be a valid size.
     * \returns                         Preferred size for the given constraint.
     */
    virtual QSizeF constrainedSize(const QSizeF constraint, Qt::WindowFrameSection pinSection) const = 0;

    static QRectF constrainedGeometry(const QRectF &geometry, Qt::WindowFrameSection pinSection, const QPointF &pinPoint, const QSizeF &constrainedSize);

protected:
    virtual QRectF constrainedGeometry(const QRectF &geometry, Qt::WindowFrameSection pinSection, const QPointF &pinPoint = QPointF()) const override {
        return constrainedGeometry(geometry, pinSection, pinPoint, constrainedSize(geometry.size(), pinSection));
    }
};


#endif // QN_CONSTRAINED_RESIZABLE
