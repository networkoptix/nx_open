#ifndef QN_ACCESSOR_H
#define QN_ACCESSOR_H

#include <QObject>

/**
 * Abstract accessor class. 
 */
class QnAbstractAccessor {
public:
    virtual QVariant get(const QObject *object) const = 0;

    virtual void set(QObject *object, const QVariant &value) const = 0;

    virtual ~QnAbstractAccessor() {};
};


/**
 * Accessor that wraps the given functors.
 */
template<class Getter, class Setter>
class QnAccessorAdaptor: public QnAbstractAccessor {
public:
    QnAccessorAdaptor(const Getter &getter, const Setter &setter): m_getter(getter), m_setter(setter) {}

    virtual QVariant get(const QObject *object) const override {
        return m_getter(object);
    }

    virtual void set(QObject *object, const QVariant &value) const override {
        m_setter(object, value);
    }

private:
    Getter m_getter;
    Setter m_setter;
};


/**
 * Factory function for accessor adaptors.
 * 
 * \param getter                        Getter functor to adapt.
 * \param setter                        Setter functor to adapt.
 */
template<class Getter, class Setter>
QnAbstractAccessor *newAccessor(const Getter &getter, const Setter &setter) {
    return new QnAccessorAdaptor<Getter, Setter>(getter, setter);
}


/**
 * Accessor for Qt properties. 
 */
class QnPropertyAccessor: public QnAbstractAccessor {
public:
    QnPropertyAccessor(const QByteArray &propertyName): m_propertyName(propertyName) {}

    virtual QVariant get(const QObject *object) const override {
        return object->property(m_propertyName.constData());
    }

    virtual void set(QObject *object, const QVariant &value) const override {
        object->setProperty(m_propertyName.constData(), value);
    }

private:
    QByteArray m_propertyName;
};


#endif // QN_ACCESSOR_H
