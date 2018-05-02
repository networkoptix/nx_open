#pragma once

#include <QtCore/QObject>
#include <QtCore/QMetaType>
#include <QtCore/QEasingCurve>

#include "animator_group.h"

class QGraphicsWidget;
class QEasingCurve;

class VariantAnimator;
class RectAnimator;

class WidgetAnimator: public AnimatorGroup
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * \param widget                    Widget that this animator will be assigned to.
     * \param geometryPropertyName      Property name for changing widget's geometry.
     * \param rotationPropertyName      Property name for changing widget's rotation.
     * \param parent                    Parent object.
     */
    WidgetAnimator(QGraphicsWidget* widget, const QByteArray& geometryPropertyName, 
        const QByteArray& rotationPropertyName, QObject* parent = nullptr);

    /**
     * Virtual destructor.
     */
    virtual ~WidgetAnimator();

    /**
    * Starts animated move of a widget to the given position.
    *
    * \param geometry                  Rectangle to move widget to, in scene coordinates.
    * \param rotation                  Rotation value for the widget.
    */
    void moveTo(const QRectF& geometry, qreal rotation);

    /**
     * \returns                         Widget that this animator is assigned to.
     */
    QGraphicsWidget* widget() const;

    /**
     * \returns                         Widget scaling speed, scale factor per second.
     */
    qreal scalingSpeed() const;

    /**
     * \returns                         Relative part of the widget's movement speed,
     *                                  in widgets per second.
     */
    qreal relativeMovementSpeed() const;

    /**
     * \returns                         Absolute part of the widget's movement speed,
     *                                  in scene coordinates.
     */
    qreal absoluteMovementSpeed() const;

    /**
     * \returns                         Widget rotation speed, in degrees per second.
     */
    qreal rotationSpeed() const;

    void setScalingSpeed(qreal scalingSpeed);

    void setRelativeMovementSpeed(qreal relativeMovementSpeed);

    void setAbsoluteMovementSpeed(qreal absoluteMovementSpeed);

    /**
     * \param multiplier                Widget rotation speed, in degrees per second.
     */
    void setRotationSpeed(qreal degrees);

    /**
    * \returns                         Easing curve used by this animator.
    */
    const QEasingCurve& easingCurve() const;

    /**
    * \param easingCurve               New easing curve to use.
    */
    void setEasingCurve(const QEasingCurve& easingCurve);

private:
    /** Widget geometry animation. */
    RectAnimator* m_geometryAnimator;

    /** Widget rotation animation. */
    VariantAnimator* m_rotationAnimator;
};

Q_DECLARE_METATYPE(WidgetAnimator*);
