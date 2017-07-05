#include "resource_property_adaptor.h"

#include <core/resource/resource.h>

// TODO: #vkutin #GDM #common This class seems too complicated now.
// 1. Is dynamic resource switching via setResource really needed?
// 2. Since we moved DB sync out of this class, queueing changes seems no longer needed.


// -------------------------------------------------------------------------- //
// QnAbstractResourcePropertyAdaptor
// -------------------------------------------------------------------------- //
QnAbstractResourcePropertyAdaptor::QnAbstractResourcePropertyAdaptor(
    const QString& key,
    const QVariant& defaultValue,
    QnAbstractResourcePropertyHandler *handler,
    QObject* parent)
:
    base_type(parent),
    m_key(key),
    m_defaultValue(defaultValue),
    m_handler(handler),
    m_pendingSave(0),
    m_value(defaultValue)
{
    connect(this, &QnAbstractResourcePropertyAdaptor::saveRequestQueued, this, &QnAbstractResourcePropertyAdaptor::processSaveRequests, Qt::QueuedConnection);
}

QnAbstractResourcePropertyAdaptor::~QnAbstractResourcePropertyAdaptor() {
    setResourceInternal(QnResourcePtr(), false); /* This will disconnect us from resource. */
}

const QString &QnAbstractResourcePropertyAdaptor::key() const {
    return m_key;
}

QnResourcePtr QnAbstractResourcePropertyAdaptor::resource() const {
    QnMutexLocker locker( &m_mutex );
    return m_resource;
}

void QnAbstractResourcePropertyAdaptor::setResource(const QnResourcePtr &resource) {
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
        QnMutexLocker locker( &m_mutex );

        if(m_resource == resource)
            return;

        if(m_resource) {
            disconnect(m_resource, NULL, this, NULL);
            oldResource = m_resource;
            oldSerializedValue = m_serializedValue;
        }

        m_resource = resource;

        if(m_resource)
            connect(resource, &QnResource::propertyChanged, this, &QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged);

        changed = loadValueLocked(newSerializedValue);
    }

    if(oldResource)
        processSaveRequestsNoLock(oldResource, oldSerializedValue);

    if(changed && notify)
        emit valueChanged();
}

QVariant QnAbstractResourcePropertyAdaptor::value() const {
    QnMutexLocker locker( &m_mutex );
    return m_value.isValid() ? m_value : m_defaultValue;
}

QString QnAbstractResourcePropertyAdaptor::serializedValue() const {
    QnMutexLocker locker( &m_mutex );
    return m_serializedValue.isEmpty() ? defaultSerializedValueLocked() : m_serializedValue;
}

QString QnAbstractResourcePropertyAdaptor::defaultSerializedValue() const
{
    QnMutexLocker locker( &m_mutex );
    return defaultSerializedValueLocked();
}

QString QnAbstractResourcePropertyAdaptor::defaultSerializedValueLocked() const
{
    return QString();
}

void QnAbstractResourcePropertyAdaptor::setValueInternal(const QVariant &value) {
    bool save = false;
    {
        QnMutexLocker locker( &m_mutex );

        if(m_handler->equals(m_value, value))
            return;

        m_value = value;

        if(!m_handler->serialize(m_value, &m_serializedValue))
            m_serializedValue = QString();

        save = m_resource;
    }

    if(save)
        enqueueSaveRequest();

    emit valueChanged();
}

bool QnAbstractResourcePropertyAdaptor::testAndSetValue(const QVariant &expectedValue, const QVariant &newValue) {
    bool save = false;
    {
        QnMutexLocker locker( &m_mutex );

        if(!m_handler->equals(m_value, expectedValue))
            return false;

        if(m_handler->equals(m_value, newValue))
            return true;

        m_value = newValue;

        if(!m_handler->serialize(m_value, &m_serializedValue))
            m_serializedValue = QString();

        save = m_resource;
    }

    if(save)
        enqueueSaveRequest();

    emit valueChanged();
    return true;
}

void QnAbstractResourcePropertyAdaptor::loadValue(const QString &serializedValue) {
    {
        QnMutexLocker locker( &m_mutex );
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

void QnAbstractResourcePropertyAdaptor::saveToResource()
{
    processSaveRequests();
}

bool QnAbstractResourcePropertyAdaptor::takeFromSettings(QSettings* settings)
{
    const auto value = settings->value(m_key);
    if (value.isNull())
        return false;

    setValueInternal(value);
    settings->remove(m_key);
    return true;
}

void QnAbstractResourcePropertyAdaptor::processSaveRequests()
{
    if(!m_pendingSave.loadAcquire())
        return;

    QnResourcePtr resource;
    QString serializedValue;
    {
        QnMutexLocker locker( &m_mutex );
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

void QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key)
{
    if(key == m_key)
        loadValue(resource->getProperty(key));
}
