#ifndef QN_WIDGET_ANIMATOR_H
#define QN_WIDGET_ANIMATOR_H

#include <QObject>
#include <QMetaType>
#include <ui/common/scene_utility.h>
#include "animator_group.h"

class QGraphicsWidget;
class QEasingCurve;

class QnVariantAnimator;
class QnRectAnimator;

class QnWidgetAnimator: public QnAnimatorGroup {
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
     * Virtual destructor.
     */
    virtual ~QnWidgetAnimator();

    /**
     * Starts animated move of a widget to the given position.
     * 
     * \param geometry                  Rectangle to move widget to, in scene coordinates.
     * \param rotation                  Rotation value for the widget.
     * \param curve                     Easing curve to use.
     */
    void moveTo(const QRectF &geometry, qreal rotation, const QEasingCurve &curve);

    void moveTo(const QRectF &geometry, qreal rotation);

    /**
     * \returns                         Widget that this animator is assigned to.
     */
    QGraphicsWidget *widget() const;

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

private:
    /** Widget geometry animation. */
    QnRectAnimator *m_geometryAnimator;

    /** Widget rotation animation. */
    QnVariantAnimator *m_rotationAnimator;
};

Q_DECLARE_METATYPE(QnWidgetAnimator *);

#endif // QN_WIDGET_ANIMATOR_H
