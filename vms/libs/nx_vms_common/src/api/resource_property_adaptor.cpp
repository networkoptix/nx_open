// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_property_adaptor.h"

// TODO: #vkutin #sivanov This class seems too complicated now.
// 1. Is dynamic resource switching via setResource really needed?
// 2. Since we moved DB sync out of this class, queuing changes seems no longer needed.

// -------------------------------------------------------------------------- //
// QnAbstractResourcePropertyAdaptor
// -------------------------------------------------------------------------- //
QnAbstractResourcePropertyAdaptor::QnAbstractResourcePropertyAdaptor(
    const QString& key,
    const QVariant& defaultValue,
    QnAbstractResourcePropertyHandler *handler,
    QObject* parent,
    std::function<QString()> label)
:
    base_type(parent),
    m_key(key),
    m_label(std::move(label)),
    m_defaultValue(defaultValue),
    m_handler(handler),
    m_pendingSave(0),
    m_value(defaultValue)
{
    NX_ASSERT(m_handler->serialize(m_defaultValue, &m_defaultSerializedValue));
    m_serializedValue = m_defaultSerializedValue;
}

const QString& QnAbstractResourcePropertyAdaptor::key() const
{
    return m_key;
}

QString QnAbstractResourcePropertyAdaptor::label() const
{
    if (m_label)
        return m_label();

    QString result = m_key.left(1).toUpper();
    for (int i = 1; i < m_key.length(); ++i)
    {
        if (m_key[i] == m_key[i].toUpper())
            result += " ";
        result += m_key[i];
    }
    return result;
}

QVariant QnAbstractResourcePropertyAdaptor::value() const {
    NX_MUTEX_LOCKER locker(&m_mutex);
    NX_ASSERT(m_value.isValid());
    return m_value;
}

bool QnAbstractResourcePropertyAdaptor::isDefault() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_handler->equals(m_value, m_defaultValue);
}

QString QnAbstractResourcePropertyAdaptor::serializedValue() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_serializedValue;
}

void QnAbstractResourcePropertyAdaptor::setValueInternal(const QVariant& value)
{
    QString serializedValue;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        if (!setValueInternalLocked(value))
            return;

        serializedValue = m_serializedValue;
    }

    emit valueChanged(key(), serializedValue);
}

bool QnAbstractResourcePropertyAdaptor::setValueInternalLocked(const QVariant& value)
{
    if (m_handler->equals(m_value, value))
        return false;

    m_value = value;
    if (m_handler->equals(m_value, m_defaultValue))
    {
        m_serializedValue = m_defaultSerializedValue;
    }
    else if (!NX_ASSERT(m_handler->serialize(m_value, &m_serializedValue)))
    {
        m_value = m_defaultValue;
        m_serializedValue = m_defaultSerializedValue;
    }
    return true;
}

bool QnAbstractResourcePropertyAdaptor::testAndSetValue(const QVariant &expectedValue, const QVariant &newValue)
{
    QString serializedValue;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        if (!m_handler->equals(m_value, expectedValue))
            return false;

        if (!setValueInternalLocked(newValue))
            return true;

        serializedValue = m_serializedValue;
    }

    emit valueChanged(key(), serializedValue);
    return true;
}

void QnAbstractResourcePropertyAdaptor::loadValue(const QString& serializedValue)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        QVariant value;
        if (serializedValue.isEmpty() || !m_handler->deserialize(serializedValue, &value))
            value = m_defaultValue;

        if (m_handler->equals(m_value, value))
            return;

        m_value = value;
        m_serializedValue = serializedValue;
    }

    emit valueChanged(key(), serializedValue);
}

bool QnAbstractResourcePropertyAdaptor::deserializeValue(
    const QString& serializedValue, QVariant* outValue) const
{
    return m_handler->deserialize(serializedValue, outValue);
}

bool QnAbstractResourcePropertyAdaptor::takeFromSettings(QSettings* settings, const QString& prefix)
{
    const auto key = prefix + m_key;
    const auto value = settings->value(key);
    if (value.isNull())
        return false;

    NX_VERBOSE(this, "Take value %1 = '%2' from %3", key, value, settings);
    loadValue(value.toString());
    settings->remove(key);
    return true;
}
