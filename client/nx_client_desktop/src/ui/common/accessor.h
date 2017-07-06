#ifndef QN_ACCESSOR_H
#define QN_ACCESSOR_H

#include <QtCore/QObject>
#include <QtCore/QVariant>

/**
 * Abstract accessor class. 
 */
class AbstractAccessor {
public:
    virtual QVariant get(const QObject *object) const = 0;

    virtual void set(QObject *object, const QVariant &value) const = 0;

    virtual ~AbstractAccessor() {}
};

/**
 * Accessor that wraps the given functors.
 */
template<class Getter, class Setter>
class AccessorAdaptor: public AbstractAccessor {
public:
    AccessorAdaptor(const Getter &getter, const Setter &setter): m_getter(getter), m_setter(setter) {}

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
AbstractAccessor *newAccessor(const Getter &getter, const Setter &setter) {
    return new AccessorAdaptor<Getter, Setter>(getter, setter);
}


/**
 * Accessor for Qt properties. 
 */
class PropertyAccessor: public AbstractAccessor {
public:
    using Pointer = QSharedPointer<PropertyAccessor>;
    static const Pointer create(const QByteArray& propertyName)
    {
        return Pointer(new PropertyAccessor(propertyName));
    }

    PropertyAccessor(const QByteArray &propertyName): m_propertyName(propertyName) {}

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
