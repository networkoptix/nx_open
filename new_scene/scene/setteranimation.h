#ifndef QN_SETTER_ANIMATION_H
#define QN_SETTER_ANIMATION_H

#include <QVariantAnimation>
#include <QScopedPointer>

namespace detail {
    class Setter {
    public:
        virtual void operator()(const QVariant &value) const = 0;
    };

    template<class T>
    class AnySetter: public Setter {
    public:
        AnySetter(const T &setter): m_setter(setter) {}

        virtual void operator()(const QVariant &value) const override {
            m_setter(value);
        }

    private:
        T m_setter;
    };

} // namespace detail


class SetterAnimation: public QVariantAnimation {
public:
    SetterAnimation(QObject *parent = NULL): QVariantAnimation(parent) {}

    template<class Setter>
    void setSetter(const Setter &setter) {
        m_setter.reset(new detail::AnySetter<Setter>(setter));
    }

public slots:
    void clear() {
        m_setter.reset();
    }

protected:
    virtual void updateCurrentValue(const QVariant &value) override {
        if(m_setter.isNull())
            return;

        m_setter->operator()(value);
    }

private:
    QScopedPointer<detail::Setter> m_setter;
};

#endif // QN_SETTER_ANIMATION_H
