#include "resource_property_adaptor.h"

#include <core/resource/resource.h>

#include "app_server_connection.h"

// -------------------------------------------------------------------------- //
// QnAbstractResourcePropertyAdaptor
// -------------------------------------------------------------------------- //
QnAbstractResourcePropertyAdaptor::QnAbstractResourcePropertyAdaptor(const QString &key, QObject *parent):
    base_type(parent),
    m_key(key),
    m_pendingSave(0)
{
    connect(this, &QnAbstractResourcePropertyAdaptor::saveRequestQueued, this, &QnAbstractResourcePropertyAdaptor::processSaveRequests, Qt::QueuedConnection);
}

QnAbstractResourcePropertyAdaptor::~QnAbstractResourcePropertyAdaptor() {
    return;
}

const QString &QnAbstractResourcePropertyAdaptor::key() const {
    return m_key;
}

QnResourcePtr QnAbstractResourcePropertyAdaptor::resource() const {
    QMutexLocker locker(&m_mutex);
    return m_resource;
}

void QnAbstractResourcePropertyAdaptor::setResource(const QnResourcePtr &resource) {
    QString newSerializedValue = resource ? resource->getProperty(m_key) : QString();

    bool changed;
    QnResourcePtr oldResource;
    QString oldSerializedValue;
    {
        QMutexLocker locker(&m_mutex);

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

    if(changed)
        emit valueChanged();
}

QVariant QnAbstractResourcePropertyAdaptor::value() const {
    QMutexLocker locker(&m_mutex);
    return m_value;
}

QString QnAbstractResourcePropertyAdaptor::serializedValue() const {
    QMutexLocker locker(&m_mutex);
    return m_serializedValue;
}

void QnAbstractResourcePropertyAdaptor::setValue(const QVariant &value) {
    {
        QMutexLocker locker(&m_mutex);

        if(equals(m_value, value))
            return;

        m_value = value;

        if(!serialize(m_value, &m_serializedValue))
            m_serializedValue = QString();
    }
    
    enqueueSaveRequest();

    emit valueChanged();
}

bool QnAbstractResourcePropertyAdaptor::testAndSetValue(const QVariant &expectedValue, const QVariant &newValue) {
    {
        QMutexLocker locker(&m_mutex);

        if(!equals(m_value, expectedValue))
            return false;

        if(equals(m_value, newValue))
            return true;

        m_value = newValue;

        if(!serialize(m_value, &m_serializedValue))
            m_serializedValue = QString();
    }

    enqueueSaveRequest();

    emit valueChanged();
    return true;
}

void QnAbstractResourcePropertyAdaptor::loadValue(const QString &serializedValue) {
    {
        QMutexLocker locker(&m_mutex);
        if(!loadValueLocked(serializedValue))
            return;
    }

    emit valueChanged();
}

bool QnAbstractResourcePropertyAdaptor::loadValueLocked(const QString &serializedValue) {
    if(m_serializedValue == serializedValue)
        return false;

    m_serializedValue = serializedValue;
    if(!deserialize(m_serializedValue, &m_value))
        m_value = QVariant();
    
    return true;
}

void QnAbstractResourcePropertyAdaptor::processSaveRequests() {
    if(!m_pendingSave.loadAcquire())
        return;
    
    QnResourcePtr resource;
    QString serializedValue;
    {
        QMutexLocker locker(&m_mutex);
        if(!m_resource)
            return;

        resource = m_resource;
        serializedValue = m_serializedValue;
    }

    processSaveRequestsNoLock(resource, serializedValue);
}

void QnAbstractResourcePropertyAdaptor::processSaveRequestsNoLock(const QnResourcePtr &resource, const QString &serializedValue) {
    if(!m_pendingSave.loadAcquire())
        return;

    m_pendingSave.storeRelease(0);

    resource->setProperty(m_key, serializedValue);
    QnAppServerConnectionFactory::createConnection()->saveAsync(resource->getId(), QnKvPairList() << QnKvPair(m_key, serializedValue));
}

void QnAbstractResourcePropertyAdaptor::enqueueSaveRequest() {
    if(m_pendingSave.loadAcquire())
        return;

    m_pendingSave.storeRelease(1);
    emit saveRequestQueued();
}

void QnAbstractResourcePropertyAdaptor::at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key) {
    if(key == m_key)
        loadValue(resource->getProperty(key));
}
