#ifndef QN_VIEWPORT_ANIMATOR_H
#define QN_VIEWPORT_ANIMATOR_H

#include <QObject>
#include <QMargins>
#include <QAbstractAnimation>
#include <ui/common/scene_utility.h>
#include "variant_animator.h"

class QGraphicsView;

class QnAccessorAnimation;

class QnViewportAnimator: public QObject, protected SceneUtility {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param view                      View that this viewport animator will be assigned to.
     * \param parent                    Parent object.
     */
    QnViewportAnimator(QObject *parent = NULL);

    /**
     * \returns                         View that this viewport animator is assigned to.
     */
    QGraphicsView *view() const {
        return m_view;
    }

    void setView(QGraphicsView *view);

    const QMargins &viewportMargins() const {
        return m_viewportMargins;
    }

    void setViewportMargins(const QMargins &margins) {
        m_viewportMargins = margins;
    }

    /**
     * Starts animated move of a viewport to the given rect. When animation
     * finishes, viewport's bounding rect will include the given rect.
     * 
     * Note that this function animates position and scale only. It does not
     * take rotation and more complex transformations into account.
     * 
     * \param rect                      Rectangle to move viewport to, in scene coordinates.
     * \param timeLimitMsecs            Maximal time for the transition, in milliseconds.
     */
    void moveTo(const QRectF &rect, int timeLimitMsecs = -1);

    const QRectF &targetRect() const;

    /**
     * \param multiplier                Viewport movement speed, in viewports per second.
     */
    qreal movementSpeed() const;

    /**
     * \returns                         Viewport scaling speed, factor per second.
     */
    qreal scalingSpeed() const;

    /**
     * \param multiplier                Viewport movement speed, in viewports per second.
     */
    void setMovementSpeed(qreal multiplier);

    /**
     * \param multiplier                Viewport scaling speed, factor per second.
     */
    void setScalingSpeed(qreal multiplier);

    /**
     * \returns                         Whether viewport animation is running.
     */
    bool isAnimating() const;

signals:
    void animationStarted();

    void animationFinished();

private slots:
    void at_view_destroyed();
    void at_animationGroup_stateChanged(QAbstractAnimation::State oldState, QAbstractAnimation::State newState);

private:
    /** Current graphics view. */
    QGraphicsView *m_view;

    /** Viewport margins. */
    QMargins m_viewportMargins;

    /** Movement speed, in viewports per second. */
    qreal m_movementSpeed;

    /** Logarithmic viewport scaling speed. */
    qreal m_logScalingSpeed;

    /** Target rect of current animation. */
    QRectF m_targetRect;

    /** Viewport animation group. */
    QParallelAnimationGroup *m_animationGroup;

    /** Viewport scale animation. */
    QnAccessorAnimation *m_scaleAnimation;

    /** Viewport position animation. */
    QnAccessorAnimation *m_positionAnimation;
};

#endif // QN_VIEWPORT_ANIMATOR_H
