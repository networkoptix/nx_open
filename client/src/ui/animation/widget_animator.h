#ifndef QN_WIDGET_ANIMATOR_H
#define QN_WIDGET_ANIMATOR_H

#include <QObject>
#include <QMetaType>
#include <ui/common/scene_utility.h>
#include "animator_group.h"

class QGraphicsWidget;
class QnVariantAnimator;

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
     * Starts animated move of a widget to the given position.
     * 
     * \param geometry                  Rectangle to move widget to, in scene coordinates.
     * \param rotation                  Rotation value for the widget.
     */
    void moveTo(const QRectF &geometry, qreal rotation);

    /**
     * \returns                         Widget that this animator is assigned to.
     */
    QGraphicsWidget *widget() const;

    /**
     * \returns                         Widget geometry change speed, in scene coordinates.
     */
    qreal translationSpeed() const;

    /**
     * \returns                         Widget rotation speed, in degrees per second.
     */
    qreal rotationSpeed() const;

    /**
     * \param multiplier                Widget geometry change speed, in scene coordinates.
     */
    void setTranslationSpeed(qreal points);

    /**
     * \param multiplier                Widget rotation speed, in degrees per second.
     */
    void setRotationSpeed(qreal degrees);

private:
    /** Widget geometry animation. */
    QnVariantAnimator *m_geometryAnimator;

    /** Widget rotation animation. */
    QnVariantAnimator *m_rotationAnimator;
};

Q_DECLARE_METATYPE(QnWidgetAnimator *);

#endif // QN_WIDGET_ANIMATOR_H
