#ifndef QN_RESOURCE_PROPERTY_ADAPTOR_H
#define QN_RESOURCE_PROPERTY_ADAPTOR_H

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>

#include <utils/common/json.h>
#include <utils/common/lexical.h>


// TODO: #Elric move serialization out after merge with customization branch

class QnAbstractResourcePropertyAdaptor: public Connective<QObject> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnAbstractResourcePropertyAdaptor(const QnResourcePtr &resource, const QString &key, QObject *parent = NULL);
    virtual ~QnAbstractResourcePropertyAdaptor();

    const QnResourcePtr &resource() const {
        return m_resource;
    }

    const QString &key() const {
        return m_key;
    }

    const QString &serializedValue() const {
        return m_serializedValue;
    }

    const QVariant &value() const {
        return m_value;
    }

    void setValue(const QVariant &value);

signals:
    void valueChanged();
    void valueChangedExternally();

protected:
    virtual bool serialize(const QVariant &value, QString *target) const = 0;
    virtual bool deserialize(const QString &value, QVariant *target) const = 0;
    virtual bool equals(const QVariant &l, const QVariant &r) const = 0;

    void loadValue();
    void saveValue();

private:
    Q_SLOT void at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key);

private:
    QnResourcePtr m_resource;
    QString m_key;
    QString m_serializedValue;
    QVariant m_value;
};


template<class T>
class QnResourcePropertyAdaptor: public QnAbstractResourcePropertyAdaptor {
    typedef QnAbstractResourcePropertyAdaptor base_type;
public:
    QnResourcePropertyAdaptor(const QnResourcePtr &resource, const QString &key, const T &defaultValue = T(), QObject *parent = NULL): 
        QnAbstractResourcePropertyAdaptor(resource, key, parent),
        m_type(qMetaTypeId<T>()),
        m_defaultValue(defaultValue)
    {}

    const T &value() const {
        const QVariant &baseValue = base_type::value();
        if(baseValue.userType() == m_type) {
            return *static_cast<const T *>(baseValue.constData());
        } else {
            return m_defaultValue;
        }
    }

    void setValue(const T &value) {
        base_type::setValue(QVariant::fromValue(value));
    }

protected:
    virtual bool equals(const QVariant &l, const QVariant &r) const override {
        if(l.userType() != m_type || r.userType() != m_type)
            return false;

        const T &ll = *static_cast<const T *>(l.constData());
        const T &rr = *static_cast<const T *>(r.constData());
        return ll == rr;
    }

    virtual bool serialize(const QVariant &value, QString *target) const override {
        if(value.userType() == m_type) {
            return serialize(*static_cast<const T *>(value.constData()), target);
        } else {
            return false;
        }
    }

    virtual bool deserialize(const QString &value, QVariant *target) const override {
        if(target->userType() != m_type)
            *target = QVariant(m_type, NULL);

        return deserialize(value, static_cast<T *>(target->data()));
    }

    virtual bool serialize(const T &value, QString *target) const = 0;
    virtual bool deserialize(const QString &value, T *target) const = 0;

private:
    int m_type;
    T m_defaultValue;
};


template<class T>
class QnJsonResourcePropertyAdaptor: public QnResourcePropertyAdaptor<T> {
    typedef QnResourcePropertyAdaptor<T> base_type;
public:
    QnJsonResourcePropertyAdaptor(const QnResourcePtr &resource, const QString &key, const T &defaultValue = T(), QObject *parent = NULL):
        base_type(resource, key, defaultValue, parent)
    {
        base_type::loadValue();
    }

protected:
    virtual bool serialize(const T &value, QString *target) const override {
        *target = QString::fromUtf8(QJson::serialized(value));
        return true;
    }

    virtual bool deserialize(const QString &value, T *target) const {
        return QJson::deserialize(value.toUtf8(), target);
    }
};


template<class T>
class QnLexicalResourcePropertyAdaptor: public QnResourcePropertyAdaptor<T> {
    typedef QnResourcePropertyAdaptor<T> base_type;
public:
    QnLexicalResourcePropertyAdaptor(const QnResourcePtr &resource, const QString &key, const T &defaultValue = T(), QObject *parent = NULL):
        base_type(resource, key, defaultValue, parent)
    {
        base_type::loadValue();
    }

protected:
    virtual bool serialize(const T &value, QString *target) const override {
        *target = QnLexical::serialized(value);
        return true;
    }

    virtual bool deserialize(const QString &value, T *target) const {
        return QnLexical::deserialize(value, target);
    }
};


#endif // QN_RESOURCE_PROPERTY_ADAPTOR_H
