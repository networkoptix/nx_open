#ifndef QN_ANIMATOR_H
#define QN_ANIMATOR_H

#include <QObject>
#include <QScopedPointer>
#include <QVariant>
#include <QEasingCurve>
#include "abstract_animator.h"
#include "accessor.h"
#include "converter.h"

class MagnitudeCalculator;
class LinearCombinator;

class QnAbstractSetter;
class QnAbstractGetter;
class QnAbstractConverter;

/**
 * Animator that animates QObject's parameters. Parameters do not need to
 * be Qt properties, as they can be set and read using specialized accessor.
 */
class QnVariantAnimator: public QnAbstractAnimator {
    Q_OBJECT;

    typedef QnAbstractAnimator base_type;

public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent object.
     */
    QnVariantAnimator(QObject *parent = NULL);

    /**
     * Virutual destructor.
     */
    virtual ~QnVariantAnimator();

    /**
     * \returns                         Speed of this animator, in the units of a type used for intermediate computations.
     */
    qreal speed() const {
        return m_speed;
    }

    /**
     * Note that animation's speed is measured in units of type used for 
     * intermediate computations, which may differ from the type of the parameter 
     * being animated if converter is set.
     *
     * \param speed                     New speed of this animator, in the units of a type used for intermediate computations.
     */
    void setSpeed(qreal speed);

    /**
     * \returns                         Accessor used by this animator, of NULL if none.
     */
    QnAbstractAccessor *accessor() const {
        return m_accessor.data();
    }

    /**
     * \param accessor                  Accessor to use. Animator will take ownership of the given accessor.
     */
    void setAccessor(QnAbstractAccessor *accessor);

    /**
     * \returns                         Converter used by this animator, or NULL if none.
     */
    QnAbstractConverter *converter() const {
        return m_converter.data();
    }

    /**
     * Sets the converter for this animator.
     * 
     * All internal computations will be performed in converter's target type.
     * Values of converter's source type will be fed to the setter and accepted 
     * from the outside world.
     *
     * This may be useful when animating integer or bounded types.
     * The drawback is that speed must be specified in terms of type used for
     * internal computations (converter's target type).
     * 
     * \param converter                 Converter to use.
     */
    void setConverter(QnAbstractConverter *converter);

    /**
     * \returns                         Easing curve used by this animator.
     */
    const QEasingCurve &easingCurve() const {
        return m_easingCurve;
    }

    /**
     * \param easingCurve               New easing curve to use.
     */
    void setEasingCurve(const QEasingCurve &easingCurve);

    /**
     * \returns                         Object being animated, or NULL if none.
     */
    QObject *targetObject() const {
        return m_target;
    }

    /**
     * \param target                    New object to animate.
     */
    void setTargetObject(QObject *target);

    /**
     * \returns                         Target value of the current animation.
     */
    QVariant targetValue() const;

    /**
     * \returns                         Starting value of the current animation.
     */
    QVariant startValue() const;

    /**
     * \param targetValue               New target value to animate to.
     */
    void setTargetValue(const QVariant &targetValue);

protected:
    MagnitudeCalculator *magnitudeCalculator() const {
        return m_magnitudeCalculator;
    }

    LinearCombinator *linearCombinator() const {
        return m_linearCombinator;
    }

    int internalType() const;

    int externalType() const;

    QVariant toInternal(const QVariant &external) const;

    QVariant toExternal(const QVariant &internal) const;

    const QVariant &internalTargetValue() const {
        return m_internalTargetValue;
    }

    const QVariant &internalStartValue() const {
        return m_internalStartValue;
    }

    virtual int estimatedDuration() const override;

    virtual void updateCurrentTime(int currentTime) override;

    virtual QVariant currentValue() const;

    virtual void updateCurrentValue(const QVariant &value) const;

    virtual void updateTargetValue(const QVariant &newTargetValue);

    virtual void updateInternalType(int newType);

    virtual void updateState(State newState) override;

    virtual QVariant interpolated(const QVariant &from, const QVariant &to, qreal progress) const;

private slots:
    void at_target_destroyed();

private:
    void setTargetObjectInternal(QObject *target);

    void setInternalTypeInternal(int newInternalType);

private:
    QScopedPointer<QnAbstractAccessor> m_accessor;
    QScopedPointer<QnAbstractConverter> m_converter;
    QEasingCurve m_easingCurve;
    int m_internalType;
    QVariant m_internalStartValue;
    QVariant m_internalTargetValue;
    QObject *m_target;
    qreal m_speed;
    MagnitudeCalculator *m_magnitudeCalculator;
    LinearCombinator *m_linearCombinator;
};

#endif // QN_ANIMATOR_H
