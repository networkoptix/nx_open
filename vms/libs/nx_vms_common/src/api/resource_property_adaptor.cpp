// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_property_adaptor.h"

#include <core/resource/resource.h>

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
    connect(this, &QnAbstractResourcePropertyAdaptor::saveRequestQueued,
        this, &QnAbstractResourcePropertyAdaptor::processSaveRequests, Qt::DirectConnection);
}

QnAbstractResourcePropertyAdaptor::~QnAbstractResourcePropertyAdaptor()
{
    setResourceInternal(QnResourcePtr(), false); /* This will disconnect us from resource. */
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

QnResourcePtr QnAbstractResourcePropertyAdaptor::resource() const
{
    NX_MUTEX_LOCKER locker( &m_mutex );
    return m_resource;
}

void QnAbstractResourcePropertyAdaptor::setResource(const QnResourcePtr &resource)
{
    setResourceInternal(resource, true);
}

void QnAbstractResourcePropertyAdaptor::setResourceInternal(const QnResourcePtr &resource, bool notify)
{
    QString newSerializedValue = resource
        ? resource->getProperty(m_key)
        : QString();
    if (newSerializedValue.isEmpty())
        newSerializedValue = defaultSerializedValue();

    bool changed;
    QnResourcePtr oldResource;
    QString oldSerializedValue;
    {
        NX_MUTEX_LOCKER locker( &m_mutex );

        if(m_resource == resource)
            return;

        if(m_resource) {
            directDisconnectAll();
            oldResource = m_resource;
            oldSerializedValue = m_serializedValue;
        }

        m_resource = resource;

        if(m_resource)
        {
            Qn::directConnect(
                resource.get(), &QnResource::propertyChanged,
                this, &QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged);
        }

        changed = loadValueLocked(newSerializedValue);
    }

    if(oldResource)
        processSaveRequestsNoLock(oldResource, oldSerializedValue);

    if(changed && notify)
        emit valueChanged();
}

QVariant QnAbstractResourcePropertyAdaptor::value() const {
    NX_MUTEX_LOCKER locker( &m_mutex );
    return m_value.isValid() ? m_value : m_defaultValue;
}

bool QnAbstractResourcePropertyAdaptor::isDefault() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_value.isNull() || m_value == m_defaultValue;
}

QString QnAbstractResourcePropertyAdaptor::serializedValue() const {
    NX_MUTEX_LOCKER locker( &m_mutex );
    return m_serializedValue.isEmpty() ? defaultSerializedValueLocked() : m_serializedValue;
}

QString QnAbstractResourcePropertyAdaptor::defaultSerializedValue() const
{
    NX_MUTEX_LOCKER locker( &m_mutex );
    return defaultSerializedValueLocked();
}

QString QnAbstractResourcePropertyAdaptor::defaultSerializedValueLocked() const
{
    return QString();
}

void QnAbstractResourcePropertyAdaptor::setValueInternal(const QVariant &value) {
    bool save = false;
    {
        NX_MUTEX_LOCKER locker( &m_mutex );

        if(m_handler->equals(m_value, value))
            return;

        m_value = value;

        if(!m_handler->serialize(m_value, &m_serializedValue))
            m_serializedValue = QString();

        save = (bool) m_resource;
    }

    if(save)
        enqueueSaveRequest();

    emit valueChanged();
}

bool QnAbstractResourcePropertyAdaptor::testAndSetValue(const QVariant &expectedValue, const QVariant &newValue) {
    bool save = false;
    {
        NX_MUTEX_LOCKER locker( &m_mutex );

        if(!m_handler->equals(m_value, expectedValue))
            return false;

        if(m_handler->equals(m_value, newValue))
            return true;

        m_value = newValue;

        if(!m_handler->serialize(m_value, &m_serializedValue))
            m_serializedValue = QString();

        save = (bool) m_resource;
    }

    if(save)
        enqueueSaveRequest();

    emit valueChanged();
    return true;
}

void QnAbstractResourcePropertyAdaptor::loadValue(const QString &serializedValue) {
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        if(!loadValueLocked(serializedValue))
            return;
    }

    emit valueChanged();
}

bool QnAbstractResourcePropertyAdaptor::loadValueLocked(const QString &serializedValue) {
    QString newSerializedValue = serializedValue.isEmpty()
        ? defaultSerializedValueLocked()
        : serializedValue;

    QString actualSerializedValue = m_serializedValue.isEmpty()
        ? defaultSerializedValueLocked()
        : m_serializedValue;

    if (actualSerializedValue == newSerializedValue)
        return false;

    m_serializedValue = newSerializedValue;
    if(m_serializedValue.isEmpty() || !m_handler->deserialize(m_serializedValue, &m_value))
        m_value = QVariant();

    return true;
}

void QnAbstractResourcePropertyAdaptor::setSerializedValue(const QVariant& value)
{
    bool save = false;
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        if (!loadValueLocked(value.toString()))
            return;
        save = !m_resource.isNull();
    }
    if (save)
        enqueueSaveRequest();

    emit valueChanged();
}

bool QnAbstractResourcePropertyAdaptor::deserializeValue(
    const QString& serializedValue, QVariant* outValue) const
{
    return m_handler->deserialize(serializedValue, outValue);
}

void QnAbstractResourcePropertyAdaptor::saveToResource()
{
    processSaveRequests();
}

bool QnAbstractResourcePropertyAdaptor::takeFromSettings(QSettings* settings, const QString& preffix)
{
    const auto key = preffix + m_key;
    const auto value = settings->value(key);
    if (value.isNull())
        return false;

    NX_VERBOSE(this, "Take value %1 = '%2' from %3", key, value, settings);
    setSerializedValue(value);
    settings->remove(key);
    return true;
}

void QnAbstractResourcePropertyAdaptor::processSaveRequests()
{
    if(!m_pendingSave.loadAcquire())
        return;

    QnResourcePtr resource;
    QString serializedValue;
    {
        NX_MUTEX_LOCKER locker( &m_mutex );
        if(!m_resource)
            return;

        resource = m_resource;
        serializedValue = m_serializedValue;
    }

    processSaveRequestsNoLock(resource, serializedValue);
}

void QnAbstractResourcePropertyAdaptor::processSaveRequestsNoLock(const QnResourcePtr &resource, const QString &serializedValue)
{
    if(!m_pendingSave.loadAcquire())
        return;

    m_pendingSave.storeRelease(0);

    resource->setProperty(m_key, serializedValue);

    emit synchronizationNeeded(resource);
}

void QnAbstractResourcePropertyAdaptor::enqueueSaveRequest()
{
    if(m_pendingSave.loadAcquire())
        return;

    m_pendingSave.storeRelease(1);
    emit saveRequestQueued();
}

void QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged(
    const QnResourcePtr& resource,
    const QString& key,
    const QString& /*prevValue*/,
    const QString& /*newValue*/)
{
    if (key == m_key)
        loadValue(resource->getProperty(key));
}
