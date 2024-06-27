// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QSettings>

#include <QtCore/QAtomicInt>

#include <core/resource/resource_fwd.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/lexical_functions.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/safe_direct_connection.h>

// TODO: #sivanov Create meta_functions and move json_serializer, lexical_serializer,
// linear_combinator & magnitude_calculator there.
// TODO: #sivanov Use equals from Qt5.2 variant.

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
    QnResourcePropertyHandler(): m_type(QMetaType::fromType<T>()) {}

    virtual bool equals(const QVariant &l, const QVariant &r) const override {
        if (l.userType() != m_type.id() || r.userType() != m_type.id())
            return false;

        const T &ll = *static_cast<const T *>(l.constData());
        const T &rr = *static_cast<const T *>(r.constData());
        return ll == rr;
    }

    virtual bool serialize(const QVariant &value, QString *target) const override {
        if (value.userType() == m_type.id()) {
            return serialize(*static_cast<const T *>(value.constData()), target);
        } else {
            return false;
        }
    }

    virtual bool deserialize(const QString &value, QVariant *target) const override {
        if (target->userType() != m_type.id())
            *target = QVariant(m_type);

        return deserialize(value, static_cast<T *>(target->data()));
    }

    virtual bool serialize(const T &value, QString *target) const = 0;
    virtual bool deserialize(const QString &value, T *target) const = 0;

private:
    QMetaType m_type;
};

template<class T>
class QnJsonResourcePropertyHandler: public QnResourcePropertyHandler<T> {
public:
    virtual bool serialize(const T &value, QString *target) const override {
        *target = QString::fromUtf8(QJson::serialized(value));
        return true;
    }

    virtual bool deserialize(const QString &value, T *target) const override {
        QnJsonContext ctx;
        ctx.setAllowStringConversions(true);
        ctx.setStrictMode(true);
        return QJson::deserialize(&ctx, value.toUtf8(), target);
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

template<typename T>
class QnReflectLexicalResourcePropertyHandler: public QnResourcePropertyHandler<T>
{
public:
    virtual bool serialize(const T& value, QString* target) const override
    {
        *target = QString::fromStdString(nx::reflect::toString(value));
        return true;
    }

    virtual bool deserialize(const QString& value, T* target) const override
    {
        return nx::reflect::fromString(value.toStdString(), target);
    }
};

/**
 * Base class for accessing resource properties.
 *
 * This class is thread-safe.
 */
class NX_VMS_COMMON_API QnAbstractResourcePropertyAdaptor:
    public QObject,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnAbstractResourcePropertyAdaptor(
        const QString& key,
        const QVariant& defaultValue,
        QnAbstractResourcePropertyHandler* handler,
        QObject* parent = NULL,
        std::function<QString()> label = nullptr);
    virtual ~QnAbstractResourcePropertyAdaptor();

    const QString& key() const;
    QString label() const;

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr &resource);

    QVariant value() const;
    bool isDefault() const; //< True if no explicit (not default) value has been set.
    QString serializedValue() const;
    bool deserializeValue(const QString& serializedValue, QVariant* outValue) const;
    virtual QJsonValue jsonValue(QnJsonContext* context = nullptr) const = 0;

    bool testAndSetValue(const QVariant &expectedValue, const QVariant &newValue);
    virtual void setValue(const QVariant& value) = 0;
    virtual void setJsonValue(const QJsonValue& value) = 0;
    virtual void setSerializedValue(const QVariant& value);

    virtual bool isSerializedValueValid(const QString& value) const = 0;
    virtual bool isJsonValueValid(const QJsonValue& value) const = 0;

    void saveToResource();
    bool takeFromSettings(QSettings* settings, const QString& preffix);

    bool isReadOnly() const { return m_isReadOnly; }
    bool isWriteOnly() const { return m_isWriteOnly; }
    bool isHidden() const { return isReadOnly() && isWriteOnly(); }
    bool isSecurity() const { return m_isSecurity; }

    void markReadOnly() { m_isReadOnly = true; }
    void markWriteOnly() { m_isWriteOnly = true; }
    void markSecurity() { m_isSecurity = true; }

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

    Q_SLOT void at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key, const QString& prevValue, const QString& newValue);

private:
    const QString m_key;
    const std::function<QString()> m_label;
    const QVariant m_defaultValue;
    const QScopedPointer<QnAbstractResourcePropertyHandler> m_handler;
    QAtomicInt m_pendingSave;

    mutable nx::Mutex m_mutex;
    QnResourcePtr m_resource;
    QString m_serializedValue;
    QVariant m_value;

    bool m_isReadOnly = false;
    bool m_isWriteOnly = false;
    bool m_isSecurity = false;
};

template<class T>
class QnResourcePropertyAdaptor: public QnAbstractResourcePropertyAdaptor {
    typedef QnAbstractResourcePropertyAdaptor base_type;
public:
    QnResourcePropertyAdaptor(
        const QString& key,
        QnResourcePropertyHandler<T>* handler,
        const T& defaultValue,
        std::function<bool(const T&)> isValueValid = nullptr,
        QObject* parent = NULL,
        std::function<QString()> label = nullptr)
        :
        QnAbstractResourcePropertyAdaptor(
            key, QVariant::fromValue(defaultValue), handler, parent, std::move(label)),
        m_type(QMetaType::fromType<T>()),
        m_defaultValue(defaultValue),
        m_isValueValid(std::move(isValueValid))
    {
        NX_CRITICAL(this->isValueValid(m_defaultValue), QJson::serialized(m_defaultValue));
        if (handler)
            handler->serialize(QVariant::fromValue(defaultValue), &m_defaultSerializedValue);
    }

    QnResourcePropertyAdaptor(
        const QString& key,
        QnResourcePropertyHandler<T>* handler,
        const T& defaultValue,
        QObject* parent,
        std::function<QString()> label = nullptr)
        :
        QnResourcePropertyAdaptor(key, handler, defaultValue, nullptr, parent, std::move(label))
    {
    }

    T value() const
    {
        QVariant baseValue = base_type::value();
        if (baseValue.userType() == m_type.id())
        {
            if (auto v = baseValue.value<T>(); NX_ASSERT(isValueValid(v), "%1 = %2", key(), baseValue))
                return v;
        }

        return m_defaultValue;
    }

    virtual QJsonValue jsonValue(QnJsonContext* context = nullptr) const override
    {
        QJsonValue result;

        if (context)
            QJson::serialize(context, value(), &result);
        else
            QJson::serialize(value(), &result);

        return result;
    }

    virtual void setValue(const QVariant& value) override
    {
        //converting incoming value to expected type
        base_type::setValueInternal(QVariant::fromValue(value.value<T>()));
    }

    virtual void setJsonValue(const QJsonValue& value) override
    {
        if (const auto typedValue = QJson::deserializeOrThrow<T>(value); isValueValid(typedValue))
            return setValue(typedValue);

        throw QJson::InvalidParameterException(std::pair<QString, QString>(
            key(), QString(QJson::serialized(value))));
    }

    void setValue(const T& value)
    {
        NX_ASSERT(isValueValid(value), "%1 = %2", key(), QJson::serialized(m_defaultValue));
        base_type::setValueInternal(QVariant::fromValue(value));
    }

    bool testAndSetValue(const T& expectedValue, const T& newValue)
    {
        return base_type::testAndSetValue(
            QVariant::fromValue(expectedValue), QVariant::fromValue(newValue));
    }

    bool isValueValid(const T& value) const
    {
        return m_isValueValid ? m_isValueValid(value) : true;
    }

    virtual bool isSerializedValueValid(const QString& serializedValue) const override
    {
        QVariant value(m_type, nullptr);
        return deserializeValue(serializedValue, &value) && isValueValid(value.value<T>());
    }

    virtual bool isJsonValueValid(const QJsonValue& value) const
    {
        T typedValue;
        return QJson::deserialize<T>(value, &typedValue) && isValueValid(typedValue);
    }

protected:
    virtual QString defaultSerializedValueLocked() const override
    {
        return m_defaultSerializedValue;
    }

private:
    const QMetaType m_type;
    const T m_defaultValue;
    const std::function<bool(const T&)> m_isValueValid;
    QString m_defaultSerializedValue;
};

template<class T>
class QnJsonResourcePropertyAdaptor: public QnResourcePropertyAdaptor<T>
{
public:
    template<typename... Args>
    QnJsonResourcePropertyAdaptor(const QString &key, Args... args):
        QnResourcePropertyAdaptor<T>(
            key, new QnJsonResourcePropertyHandler<T>(), std::forward<Args>(args)...)
    {
    }
};

template<class T>
class QnLexicalResourcePropertyAdaptor: public QnResourcePropertyAdaptor<T>
{
public:
    template<typename... Args>
    QnLexicalResourcePropertyAdaptor(const QString &key, Args... args):
        QnResourcePropertyAdaptor<T>(
            key, new QnLexicalResourcePropertyHandler<T>(), std::forward<Args>(args)...)
    {
    }
};

template<typename T>
class QnReflectLexicalResourcePropertyAdaptor: public QnResourcePropertyAdaptor<T>
{
public:
    template<typename... Args>
    QnReflectLexicalResourcePropertyAdaptor(const QString& key, Args... args):
        QnResourcePropertyAdaptor<T>(
            key, new QnReflectLexicalResourcePropertyHandler<T>(), std::forward<Args>(args)...)
    {
    }
};
