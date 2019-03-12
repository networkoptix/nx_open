#pragma once

#include <QtCore/QAtomicInt>
#include <QSettings>

#include <utils/common/connective.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/lexical_functions.h>
#include <nx/utils/safe_direct_connection.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/impl/ec_api_impl.h>

// TODO: #Elric create meta_functions and move json_serializer, lexical_serializer, linear_combinator & magnitude_calculator there

// TODO: #Elric use equals from Qt5.2 variant

class QnAbstractResourcePropertyHandler {
public:
    QnAbstractResourcePropertyHandler() {}
    virtual ~QnAbstractResourcePropertyHandler() {}

    virtual bool serialize(const QVariant &value, QString *target) const = 0;
    virtual bool deserialize(const QString &value, QVariant *target) const = 0;
    virtual bool equals(const QVariant &l, const QVariant &r) const = 0;
};

template<class T>
class QnResourcePropertyHandler: public QnAbstractResourcePropertyHandler {
public:
    QnResourcePropertyHandler(): m_type(qMetaTypeId<T>()) {}

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
};

template<class T>
class QnJsonResourcePropertyHandler: public QnResourcePropertyHandler<T> {
public:
    virtual bool serialize(const T &value, QString *target) const override {
        *target = QString::fromUtf8(QJson::serialized(value));
        return true;
    }

    virtual bool deserialize(const QString &value, T *target) const override {
        return QJson::deserialize(value.toUtf8(), target);
    }
};

template<class T>
class QnLexicalResourcePropertyHandler: public QnResourcePropertyHandler<T> {
public:
    virtual bool serialize(const T &value, QString *target) const override {
        *target = QnLexical::serialized(value);
        return true;
    }

    virtual bool deserialize(const QString &value, T *target) const override {
        return QnLexical::deserialize(value, target);
    }
};

/**
 * Base class for accessing resource properties.
 *
 * This class is thread-safe.
 */
class QnAbstractResourcePropertyAdaptor:
    public Connective<QObject>,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnAbstractResourcePropertyAdaptor(
        const QString &key,
        const QVariant& defaultValue,
        QnAbstractResourcePropertyHandler *handler,
        QObject *parent = NULL);
    virtual ~QnAbstractResourcePropertyAdaptor();

    const QString &key() const;

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr &resource);

    QVariant value() const;
    QString serializedValue() const;
    bool testAndSetValue(const QVariant &expectedValue, const QVariant &newValue);
    virtual void setValue(const QVariant& value) = 0;
    virtual void setSerializedValue(const QVariant& value);

    void saveToResource();
    bool takeFromSettings(QSettings* settings);

signals:
    void valueChanged();
    void synchronizationNeeded(const QnResourcePtr& resource);

protected:
    QString defaultSerializedValue() const;
    virtual QString defaultSerializedValueLocked() const;
    void setValueInternal(const QVariant& value);

private:
    void loadValue(const QString &serializedValue);
    bool loadValueLocked(const QString &serializedValue);

    void processSaveRequests();
    void processSaveRequestsNoLock(const QnResourcePtr &resource, const QString &serializedValue);
    void enqueueSaveRequest();
    Q_SIGNAL void saveRequestQueued();

    void setResourceInternal(const QnResourcePtr &resource, bool notify);

    Q_SLOT void at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key);

private:
    const QString m_key;
    const QVariant m_defaultValue;
    const QScopedPointer<QnAbstractResourcePropertyHandler> m_handler;
    QAtomicInt m_pendingSave;

    mutable QnMutex m_mutex;
    QnResourcePtr m_resource;
    QString m_serializedValue;
    QVariant m_value;
};

template<class T>
class QnResourcePropertyAdaptor: public QnAbstractResourcePropertyAdaptor {
    typedef QnAbstractResourcePropertyAdaptor base_type;
public:
    QnResourcePropertyAdaptor(const QString &key, QnResourcePropertyHandler<T> *handler, const T &defaultValue = T(), QObject *parent = NULL):
        QnAbstractResourcePropertyAdaptor(key, QVariant::fromValue(defaultValue), handler, parent),
        m_type(qMetaTypeId<T>()),
        m_defaultValue(defaultValue)
    {
        if (handler)
            handler->serialize(qVariantFromValue(defaultValue), &m_defaultSerializedValue);
    }

    T value() const {
        QVariant baseValue = base_type::value();
        if(baseValue.userType() == m_type) {
            return baseValue.value<T>();
        } else {
            return m_defaultValue;
        }
    }

    virtual void setValue(const QVariant& value) override
    {
        //converting incoming value to expected type
        base_type::setValueInternal(QVariant::fromValue(value.value<T>()));
    }

    void setValue(const T &value) {
        base_type::setValueInternal(QVariant::fromValue(value));
    }

    bool testAndSetValue(const T &expectedValue, const T &newValue) {
        return base_type::testAndSetValue(QVariant::fromValue(expectedValue), QVariant::fromValue(newValue));
    }

protected:
    virtual QString defaultSerializedValueLocked() const override {
        return m_defaultSerializedValue;
    }

private:
    const int m_type;
    const T m_defaultValue;
    QString m_defaultSerializedValue;
};

template<class T>
class QnJsonResourcePropertyAdaptor: public QnResourcePropertyAdaptor<T> {
    typedef QnResourcePropertyAdaptor<T> base_type;
public:
    QnJsonResourcePropertyAdaptor(const QString &key, const T &defaultValue = T(), QObject *parent = NULL):
        base_type(key, new QnJsonResourcePropertyHandler<T>(), defaultValue, parent)
    {}
};

template<class T>
class QnLexicalResourcePropertyAdaptor: public QnResourcePropertyAdaptor<T> {
    typedef QnResourcePropertyAdaptor<T> base_type;
public:
    QnLexicalResourcePropertyAdaptor(const QString &key, const T &defaultValue = T(), QObject *parent = NULL):
        base_type(key, new QnLexicalResourcePropertyHandler<T>(), defaultValue, parent)
    {}
};
