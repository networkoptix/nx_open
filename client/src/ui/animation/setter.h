#ifndef QN_SETTER_H
#define QN_SETTER_H

#include <QObject>

/**
 * Abstract setter class. 
 */
class QnAbstractSetter {
public:
    virtual void operator()(QObject *object, const QVariant &value) const = 0;

    virtual ~QnAbstractSetter() {}
};


/**
 * Setter that adapts the given setter functor.
 */
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


/**
 * Factory function for setter adaptors.
 * 
 * \param setter                        Setter functor to adapt.
 */
template<class Setter>
QnAbstractSetter *newSetter(const Setter &setter) {
    return new QnSetterAdaptor<Setter>(setter);
}


/**
 * Setter for Qt properties. 
 */
class QnPropertySetter: public QnAbstractSetter {
public:
    QnPropertySetter(const QByteArray &propertyName): m_propertyName(propertyName) {}

    virtual void operator()(QObject *object, const QVariant &value) const override {
        object->setProperty(m_propertyName.constData(), value);
    }

private:
    QByteArray m_propertyName;
};


/**
 * Getter for color properties that converts QVector4D to QColor.
 */
class QnColorPropertySetter: public QnPropertySetter {
public:
    QnColorPropertySetter(const QByteArray &propertyName): QnPropertySetter(propertyName) {}

    virtual void operator()(QObject *object, const QVariant &value) const override;
};


#endif // QN_SETTER_H
