#include "resource_property_adaptor.h"

#include <core/resource/resource.h>
#include "app_server_connection.h"
#include "nx_ec/data/api_resource_data.h"

// TODO: #Elric
// Right now we have a problem in case resource property is changed very often.
// Changes are pushed to Server and then we get them back as notifications, not
// necessarily in the original order. This way property value gets changed
// in totally unexpected ways.
#if 0
namespace {
    QString createAdaptorKey() {
        return lit("id%1|").arg(qrand() % 100000, 5, 10, QChar(L'0'));
    }

    bool hasAdaptorKey(const QString &string) {
        return string.size() >= 8 && string[0] == L'i' && string[1] == L'd' && string[7] == L'|';
    }

    Q_GLOBAL_STATIC_WITH_ARGS(QString, qn_resourcePropertyAdaptor_uniqueKey, createAdaptorKey())
}
#endif 


// -------------------------------------------------------------------------- //
// QnAbstractResourcePropertyAdaptor
// -------------------------------------------------------------------------- //
QnAbstractResourcePropertyAdaptor::QnAbstractResourcePropertyAdaptor(const QString &key, QnAbstractResourcePropertyHandler *handler, QObject *parent):
    base_type(parent),
    m_key(key),
    m_handler(handler),
    m_pendingSave(0)
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
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_resource;
}

void QnAbstractResourcePropertyAdaptor::setResource(const QnResourcePtr &resource) {
    setResourceInternal(resource, true);
}

void QnAbstractResourcePropertyAdaptor::setResourceInternal(const QnResourcePtr &resource, bool notify) {
    QString newSerializedValue = resource ? resource->getProperty(m_key) : defaultSerializedValue();

    bool changed;
    QnResourcePtr oldResource;
    QString oldSerializedValue;
    {
        SCOPED_MUTEX_LOCK( locker, &m_mutex);

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
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_value;
}

QString QnAbstractResourcePropertyAdaptor::serializedValue() const {
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_serializedValue;
}

QString QnAbstractResourcePropertyAdaptor::defaultSerializedValue() const {
    return QString();
}

void QnAbstractResourcePropertyAdaptor::setValue(const QVariant &value) {
    bool save = false;
    {
        SCOPED_MUTEX_LOCK( locker, &m_mutex);

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
        SCOPED_MUTEX_LOCK( locker, &m_mutex);

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
        SCOPED_MUTEX_LOCK( locker, &m_mutex);
        if(!loadValueLocked(serializedValue))
            return;
    }

    emit valueChanged();
}

bool QnAbstractResourcePropertyAdaptor::loadValueLocked(const QString &serializedValue) {
    if(m_serializedValue == serializedValue)
        return false;

    m_serializedValue = serializedValue;
    if(m_serializedValue.isEmpty() || !m_handler->deserialize(m_serializedValue, &m_value))
        m_value = QVariant();

    return true;
}

void QnAbstractResourcePropertyAdaptor::synchronizeNow() {
    processSaveRequests();
}


void QnAbstractResourcePropertyAdaptor::processSaveRequests() {
    if(!m_pendingSave.loadAcquire())
        return;

    QnResourcePtr resource;
    QString serializedValue;
    {
        SCOPED_MUTEX_LOCK( locker, &m_mutex);
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

    ec2::AbstractECConnectionPtr connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
        return;
    ec2::ApiResourceParamWithRefDataList params;
    params.push_back(ec2::ApiResourceParamWithRefData(resource->getId(), m_key, serializedValue));
    connection->getResourceManager()->save(params, this, &QnAbstractResourcePropertyAdaptor::at_connection_propertiesSaved);
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

void QnAbstractResourcePropertyAdaptor::at_connection_propertiesSaved(int, ec2::ErrorCode) {
    return;
}
