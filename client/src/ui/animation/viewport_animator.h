#ifndef QN_VIEWPORT_ANIMATOR_H
#define QN_VIEWPORT_ANIMATOR_H

#include <QObject>
#include <ui/common/scene_utility.h>
#include "rect_animator.h"
#include "viewport_geometry_accessor.h"

class QGraphicsView;
class QMargins;

class ViewportAnimator: public RectAnimator {
    Q_OBJECT;

    typedef RectAnimator base_type;

public:

    /**
     * Constructor.
     * 
     * \param view                      View that this viewport animator will be assigned to.
     * \param parent                    Parent object.
     */
    ViewportAnimator(QObject *parent = NULL);

    virtual ~ViewportAnimator();

    /**
     * \returns                         View that this viewport animator is assigned to.
     */
    QGraphicsView *view() const;

    void setView(QGraphicsView *view);

    const QMargins &viewportMargins() const;

    void setViewportMargins(const QMargins &margins);

    Qn::MarginFlags marginFlags() const;

    void setMarginFlags(Qn::MarginFlags marginFlags);

    /**
     * Starts animated move of a viewport to the given rect. When animation
     * finishes, viewport's bounding rect will include the given rect.
     * 
     * Note that this function animates position and scale only. It does not
     * take rotation and more complex transformations into account.
     * 
     * \param rect                      Rectangle to move viewport to, in scene coordinates.
     */
    void moveTo(const QRectF &rect);

    QRectF targetRect() const;

protected:
    virtual int estimatedDuration(const QVariant &from, const QVariant &to) const override;

private:
    /** Accessor for viewport rect. */
    ViewportGeometryAccessor *m_accessor;

    qreal m_relativeSpeed;
};

#endif // QN_VIEWPORT_ANIMATOR_H
