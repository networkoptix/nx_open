#ifndef QN_SETTER_ANIMATION_H
#define QN_SETTER_ANIMATION_H

#include <QVariantAnimation>
#include <QScopedPointer>
#include <QWeakPointer>
#include "setter.h"

class QnSetterAnimation: public QVariantAnimation {
    Q_OBJECT;
public:
    QnSetterAnimation(QObject *parent = NULL): QVariantAnimation(parent) {}

    QnAbstractSetter *setter() const {
        return m_setter.data();
    }

    void setSetter(QnAbstractSetter *setter) {
        m_setter.reset(setter);
    }

    QObject *targetObject() const {
        return m_target.data();
    }

    void setTargetObject(QObject *target) {
        m_target = target;
    }

public slots:
    void clear() {
        m_setter.reset();
        m_target.clear();
    }

protected:
    virtual void updateCurrentValue(const QVariant &value) override {
        if(m_setter.isNull())
            return;

        if(m_target.isNull())
            return;

        m_setter->operator()(m_target.data(), value);
    }

private:
    QScopedPointer<QnAbstractSetter> m_setter;
    QWeakPointer<QObject> m_target;
};

#endif // QN_SETTER_ANIMATION_H
