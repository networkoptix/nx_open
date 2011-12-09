#ifndef QN_GETTER_H
#define QN_GETTER_H

#include <QObject>

/**
 * Abstract getter class. 
 */
class QnAbstractGetter {
public:
    virtual QVariant operator()(const QObject *object) const = 0;

    virtual ~QnAbstractGetter() {};
};


/**
 * Getter that wraps the given getter functor.
 */
template<class Getter>
class QnGetterAdaptor: public QnAbstractGetter {
public:
    QnGetterAdaptor(const Getter &getter): m_getter(getter) {}

    virtual QVariant operator()(const QObject *object) const override {
        return m_getter(object);
    }

private:
    Getter m_getter;
};


/**
 * Factory function for getter adaptors.
 * 
 * \param getter                        Getter functor to adapt.
 */
template<class Getter>
QnAbstractGetter *newGetter(const Getter &getter) {
    return new QnGetterAdaptor<Getter>(getter);
}


/**
 * Getter for Qt properties. 
 */
class QnPropertyGetter: public QnAbstractGetter {
public:
    QnPropertyGetter(const QByteArray &propertyName): m_propertyName(propertyName) {}

    virtual QVariant operator()(const QObject *object) const override {
        return object->property(m_propertyName.constData());
    }

private:
    QByteArray m_propertyName;
};


/**
 * Getter for color properties that converts QColor to QVector4D.
 */
class QnColorPropertyGetter: public QnPropertyGetter {
public:
    QnColorPropertyGetter(const QByteArray &propertyName): QnPropertyGetter(propertyName) {}

    virtual QVariant operator()(const QObject *object) const override;
};


#endif // QN_GETTER_H
