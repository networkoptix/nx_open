#ifndef QN_SETTER_ANIMATION_H
#define QN_SETTER_ANIMATION_H

#include <QVariantAnimation>
#include <QScopedPointer>


class QnAbstractSetter {
public:
    virtual void operator()(QObject *object, const QVariant &value) const = 0;

    virtual ~QnAbstractSetter() {}
};


class QnAbstractGetter {
    virtual QVariant operator()(QObject *object) const = 0;

    virtual ~QnAbstractGetter() {};
};


template<class Setter>
class QnSetterAdaptor: public QnAbstractSetter {
public:
    QnSetterAdaptor(const Setter &setter): m_setter(setter) {}

    virtual void operator()(QObject *object, const QVariant &value) const override {
        m_setter(object, value);
    }

private:
    Setter m_setter;
};

template<class Setter>
QnAbstractSetter *newSetter(const Setter &setter) {
    return new QnSetterAdaptor<Setter>(setter);
}

template<class Getter>
class QnGetterAdaptor: public QnAbstractGetter {
public:
    QnGetterAdaptor(const Getter &getter): m_getter(getter) {}

    virtual QVariant operator()(QObject *object) const override {
        return m_getter(object);
    }

private:
    Getter m_getter;
};

template<class Getter>
QnAbstractGetter *newGetter(const Getter &setter) {
    return new QnGetterAdaptor<Getter>(setter);
}


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
