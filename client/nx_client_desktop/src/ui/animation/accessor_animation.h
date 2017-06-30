#pragma once

#include <QtCore/QVariantAnimation>
#include <QtCore/QScopedPointer>
#include <QtCore/QPointer>

#include <ui/common/accessor.h>

class QnAccessorAnimation: public QVariantAnimation {
    Q_OBJECT;
public:
    QnAccessorAnimation(QObject *parent = NULL): QVariantAnimation(parent) {}

    AbstractAccessor *accessor() const {
        return m_accessor.data();
    }

    void setAccessor(AbstractAccessor *accessor) {
        m_accessor.reset(accessor);
    }

    QObject *targetObject() const {
        return m_target.data();
    }

    void setTargetObject(QObject *target) {
        m_target = target;
    }

public slots:
    void clear() {
        m_accessor.reset();
        m_target.clear();
    }

protected:
    virtual void updateCurrentValue(const QVariant &value) override {
        if(m_accessor.isNull())
            return;

        if(m_target.isNull())
            return;

        m_accessor->set(m_target.data(), value);
    }

private:
    QScopedPointer<AbstractAccessor> m_accessor;
    QPointer<QObject> m_target;
};
