#ifndef QN_ANIMATOR_H
#define QN_ANIMATOR_H

#include <QObject>
#include <QScopedPointer>
#include <QVariant>
#include "abstract_animator.h"
#include "accessor.h"
#include "converter.h"

class MagnitudeCalculator;
class LinearCombinator;

class QnAbstractSetter;
class QnAbstractGetter;
class QnAbstractConverter;

class QnVariantAnimator: public QnAbstractAnimator {
    Q_OBJECT;

    typedef QnAbstractAnimator base_type;

public:
    QnVariantAnimator(QObject *parent = NULL);

    virtual ~QnVariantAnimator();

    qreal speed() const {
        return m_speed;
    }

    void setSpeed(qreal speed);

    QnAbstractAccessor *accessor() const {
        return m_accessor.data();
    }

    void setAccessor(QnAbstractAccessor *accessor);

    QnAbstractConverter *converter() const {
        return m_converter.data();
    }

    void setConverter(QnAbstractConverter *converter);

    QObject *targetObject() const {
        return m_target;
    }

    void setTargetObject(QObject *target);

    int type() const {
        return m_type;
    }

    const QVariant &targetValue() const {
        return m_targetValue;
    }

    const QVariant &startValue() const {
        return m_startValue;
    }

    void setTargetValue(const QVariant &targetValue);

protected:
    virtual int estimatedDuration() const override;

    virtual void updateCurrentTime(int currentTime) override;

    virtual QVariant currentValue() const;

    virtual void updateCurrentValue(const QVariant &value) const;

    virtual void updateTargetValue(const QVariant &newTargetValue);

    virtual void updateType(int newType);

    virtual void updateState(State newState) override;

    virtual QVariant interpolated(const QVariant &from, const QVariant &to, qreal progress) const;

    void setType(int newType);

private slots:
    void at_target_destroyed();

private:
    void setTargetObjectInternal(QObject *target);

private:
    QScopedPointer<QnAbstractAccessor> m_accessor;
    QScopedPointer<QnAbstractConverter> m_converter;
    int m_type;
    QVariant m_startValue;
    QVariant m_targetValue;
    QObject *m_target;
    qreal m_speed;
    MagnitudeCalculator *m_magnitudeCalculator;
    LinearCombinator *m_linearCombinator;
};

#endif // QN_ANIMATOR_H
