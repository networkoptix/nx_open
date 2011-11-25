#ifndef QN_SETTER_ANIMATION_H
#define QN_SETTER_ANIMATION_H

#include <QVariantAnimation>
#include <QScopedPointer>
#include "setter.h"

class SetterAnimation: public QVariantAnimation {
public:
    SetterAnimation(QObject *parent = NULL): QVariantAnimation(parent) {}

    void setSetter(QnAbstractSetter *setter) {
        m_setter.reset(setter);
    }

    void setTargetObject(QObject *target) {
        m_target = target;
    }

public slots:
    void clear() {
        m_setter.reset();
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
