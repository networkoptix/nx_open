#ifndef QN_WIDGET_ANIMATOR_H
#define QN_WIDGET_ANIMATOR_H

#include <QObject>
#include <utils/common/scene_utility.h>

class QGraphicsWidget;
class QPropertyAnimation;
class QParallelAnimationGroup;

class QnWidgetAnimator: public QObject, protected QnSceneUtility {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param widget                    Widget that this animator will be assigned to.
     * \param geometryPropertyName      Property name for changing widget's geometry.
     * \param rotationPropertyName      Property name for changing widget's rotation.
     * \param parent                    Parent object.
     */
    QnWidgetAnimator(QGraphicsWidget *widget, const QByteArray &geometryPropertyName, const QByteArray &rotationPropertyName, QObject *parent = NULL);

    /**
     * Starts animated move of a widget to the given position.
     * 
     * \param geometry                  Rectangle to move widget to, in scene coordinates.
     * \param rotation                  Rotation value for the widget.
     * \param timeLimitMsecs            Maximal time for the transition, in milliseconds.
     */
    void moveTo(const QRectF &geometry, qreal rotation, int timeLimitMsecs = -1);

    /**
     * Starts animated move of a widget to the given position.
     * 
     * \param geometry                  Rectangle to move widget to, in scene coordinates.
     * \param rotation                  Rotation value for the widget.
     * \param z                         Z value to set for the widget when animation finishes. 
     *                                  Note that widget's z value won't be animated.
     * \param timeLimitMsecs            Maximal time for the transition, in milliseconds.
     */
    void moveTo(const QRectF &geometry, qreal rotation, qreal z, int timeLimitMsecs = -1);

    /**
     * \param multiplier                Widget movement speed, in widgets per second.
     */
    qreal movementSpeed() const;

    /**
     * \returns                         Widget scaling speed, factor per second.
     */
    qreal scalingSpeed() const;

    /**
     * \returns                         Widget rotation speed, in degrees per second.
     */
    qreal rotationSpeed() const;

    /**
     * \returns                         Widget that this animator is assigned to.
     */
    QGraphicsWidget *widget() const;

    /**
     * \param multiplier                Widget movement speed, in widgets per second.
     */
    void setMovementSpeed(qreal multiplier);

    /**
     * \param multiplier                Widget scaling speed, factor per second.
     */
    void setScalingSpeed(qreal multiplier);

    /**
     * \param multiplier                Widget rotation speed, in degrees per second.
     */
    void setRotationSpeed(qreal degrees);

    /**
     * \returns                         Whether animation is running.
     */
    bool isAnimating() const;

    /**
     * Stops animation.
     */
    void stopAnimation();

signals:
    void animationStarted();

    void animationFinished();

private slots:
    void at_widget_destroyed();
    void at_animation_finished();

private:
    /** Current graphics widget. */
    QGraphicsWidget *m_widget;

    /** Movement speed, in widgets per second. */
    qreal m_movementSpeed;

    /** Logarithmic scaling speed. */
    qreal m_logScalingSpeed;

    /** Rotation speed, in degrees per second. */
    qreal m_rotationSpeed;

    /** Whether to set z value when animation finishes. */
    bool m_haveZ;

    /** Z value to set for the widget when animation finishes. */
    qreal m_z;

    /** Viewport animation group. */
    QParallelAnimationGroup *m_animationGroup;

    /** Widget geometry animation. */
    QPropertyAnimation *m_geometryAnimation;

    /** Widget rotation animation. */
    QPropertyAnimation *m_rotationAnimation;
};


#endif // QN_WIDGET_ANIMATOR_H
