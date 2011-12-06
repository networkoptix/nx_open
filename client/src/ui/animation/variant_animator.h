#ifndef QN_ANIMATOR_H
#define QN_ANIMATOR_H

#include <QObject>
#include <QScopedPointer>
#include <QVariant>
#include "abstract_animator.h"

class MagnitudeCalculator;
class LinearCombinator;

class QnAbstractSetter;
class QnAbstractGetter;

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

    QnAbstractGetter *getter() const {
        return m_getter.data();
    }

    void setGetter(QnAbstractGetter *getter);

    QnAbstractSetter *setter() const {
        return m_setter.data();
    }

    void setSetter(QnAbstractSetter *setter);

    QObject *targetObject() const {
        return m_target;
    }

    void setTargetObject(QObject *target);

protected:
    virtual QVariant currentValue() const override;

    virtual void updateCurrentValue(const QVariant &value) const override;

    virtual QVariant interpolated(int deltaTime) const override;

    virtual int requiredTime(const QVariant &from, const QVariant &to) const override;

    virtual void updateState(State newState);

    virtual void updateType(int newType);

private slots:
    void at_target_destroyed();

private:
    void setTargetObjectInternal(QObject *target);

private:
    QScopedPointer<QnAbstractGetter> m_getter;
    QScopedPointer<QnAbstractSetter> m_setter;
    QObject *m_target;
    qreal m_speed;
    QVariant m_delta;
    MagnitudeCalculator *m_magnitudeCalculator;
    LinearCombinator *m_linearCombinator;
};



#endif // QN_ANIMATOR_H
