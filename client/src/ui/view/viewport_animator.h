#ifndef QN_VIEWPORT_ANIMATOR_H
#define QN_VIEWPORT_ANIMATOR_H

#include <QObject>
#include <utils/common/scene_utility.h>

class QParallelAnimationGroup;
class QGraphicsView;

class SetterAnimation;

class QnViewportAnimator: public QObject, protected QnSceneUtility {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param view                      View that this viewport animator will be assigned to.
     * \param parent                    Parent object.
     */
    QnViewportAnimator(QGraphicsView *view, QObject *parent = NULL);

    /**
     * Starts animated move of a viewport to the given rect. When animation
     * finished, viewport's bounding rect will include the given rect.
     * 
     * Note that this function animates position and scale only. It does not
     * take rotation and more complex transformations into account.
     * 
     * \param rect                      Rectangle to move viewport to, in scene coordinates.
     * \param timeLimitMsecs            Maximal time for the transition, in milliseconds.
     */
    void moveTo(const QRectF &rect, int timeLimitMsecs = -1);

    /**
     * \param multiplier                Viewport movement speed, in viewports per second.
     */
    qreal movementSpeed() const;

    /**
     * \returns                         Viewport scaling speed, factor per second.
     */
    qreal scalingSpeed() const;

    /**
     * \returns                         View that this viewport animator is assigned to.
     */
    QGraphicsView *view() const;

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

#if 0
public slots:
    /** 
     * Reverses the current animation. Does nothing if no animation is running.
     */
    void reverse();
#endif

signals:
    void animationStarted();

    void animationFinished();

private slots:
    void at_view_destroyed();

private:
    /** Current graphics view. */
    QGraphicsView *m_view;

    /** Movement speed, in viewports per second. */
    qreal m_movementSpeed;

    /** Logarithmic viewport scaling speed. */
    qreal m_logScalingSpeed;

    /** Viewport animation group. */
    QParallelAnimationGroup *m_animationGroup;

    /** Viewport scale animation. */
    SetterAnimation *m_scaleAnimation;

    /** Viewport position animation. */
    SetterAnimation *m_positionAnimation;
};

#endif // QN_VIEWPORT_ANIMATOR_H
