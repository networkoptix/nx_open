#ifndef QN_VIEWPORT_ANIMATOR_H
#define QN_VIEWPORT_ANIMATOR_H

#include <QObject>
#include <ui/common/scene_utility.h>
#include "variant_animator.h"

class QGraphicsView;
class QMargins;

class QnViewportRectAccessor;

class QnViewportAnimator: public QnVariantAnimator {
    Q_OBJECT;

    typedef QnVariantAnimator base_type;

public:
    /**
     * Constructor.
     * 
     * \param view                      View that this viewport animator will be assigned to.
     * \param parent                    Parent object.
     */
    QnViewportAnimator(QObject *parent = NULL);

    virtual ~QnViewportAnimator();

    /**
     * \returns                         View that this viewport animator is assigned to.
     */
    QGraphicsView *view() const;

    void setView(QGraphicsView *view);

    const QMargins &viewportMargins() const;

    void setViewportMargins(const QMargins &margins);

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

    void setRelativeSpeed(qreal relativeSpeed);

    qreal relativeSpeed() const {
        return m_relativeSpeed;
    }

    QRectF targetRect() const;

protected:
    virtual int estimatedDuration() const override;

private:
    /** Accessor for viewport rect. */
    QnViewportRectAccessor *m_accessor;

    qreal m_relativeSpeed;
};

#endif // QN_VIEWPORT_ANIMATOR_H
