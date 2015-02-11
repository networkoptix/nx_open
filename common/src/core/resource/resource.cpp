#include "resource.h"

#include <climits>

#include <typeinfo>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QRunnable>

#include <utils/common/log.h>
#include <utils/common/warnings.h>
#include <utils/common/model_functions.h>

#include <core/resource/camera_advanced_param.h>
#include "core/dataprovider/abstract_streamdataprovider.h"
#include "core/resource_management/resource_pool.h"
#include "core/ptz/abstract_ptz_controller.h"

#include "resource_command_processor.h"
#include "resource_consumer.h"
#include "resource_property.h"

#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "resource_command.h"
#include "nx_ec/data/api_resource_data.h"
#include "../resource_management/resource_properties.h"
#include "../resource_management/status_dictionary.h"

bool QnResource::m_appStopping = false;
QnInitResPool QnResource::m_initAsyncPool;


static const qint64 MIN_INIT_INTERVAL = 1000000ll * 30;


// -------------------------------------------------------------------------- //
// QnResourceGetParamCommand
// -------------------------------------------------------------------------- //
#ifdef ENABLE_DATA_PROVIDERS
class QnResourceGetParamCommand : public QnResourceCommand
{
public:
    QnResourceGetParamCommand(const QnResourcePtr &res, const QString& id):
        QnResourceCommand(res),
        m_id(id)
    {}

    virtual void beforeDisconnectFromResource(){}

    bool execute()
    {
        bool success = false;
        QString value;
        if (isConnectedToTheResource()) {
            success = m_resource->getParamPhysical(m_id, value);
        }
        emit m_resource->asyncParamGetDone(m_resource, m_id, value, success);
        return success;
    }

private:
    QString m_id;
};

typedef QSharedPointer<QnResourceGetParamCommand> QnResourceGetParamCommandPtr;

// -------------------------------------------------------------------------- //
// QnResourceGetParamsCommand
// -------------------------------------------------------------------------- //
class QnResourceGetParamsCommand : public QnResourceCommand
{
public:
    QnResourceGetParamsCommand(const QnResourcePtr &res, const QSet<QString>& ids):
        QnResourceCommand(res),
        m_ids(ids)
    {}

    virtual void beforeDisconnectFromResource(){}

    bool execute()
    {
        bool success = isConnectedToTheResource();
        
        QnCameraAdvancedParamValueList result;
        if (success) {
            for(const QString &id: m_ids) {
                QString value;
                if (m_resource->getParamPhysical(id, value))
                    result << QnCameraAdvancedParamValue(id, value);
                else
                    success = false;
            }
        }
        emit m_resource->asyncParamsGetDone(m_resource, result);
        return success;
    }

private:
    QSet<QString> m_ids;
};

typedef QSharedPointer<QnResourceGetParamsCommand> QnResourceGetParamsCommandPtr;

// -------------------------------------------------------------------------- //
// QnResourceSetParamCommand
// -------------------------------------------------------------------------- //
class QnResourceSetParamCommand : public QnResourceCommand
{
public:
    QnResourceSetParamCommand(const QnResourcePtr &res, const QString& id, const QString& value):
        QnResourceCommand(res),
        m_id(id),
        m_value(value)
    {}

    virtual void beforeDisconnectFromResource(){}

    bool execute()
    {
        bool success = false;
        if (isConnectedToTheResource()) {
            success = m_resource->setParamPhysical(m_id, m_value);
        }
        emit m_resource->asyncParamSetDone(m_resource, m_id, m_value, success);
        return success;
    }

private:
    QString m_id;
    QString m_value;
};
typedef QSharedPointer<QnResourceSetParamCommand> QnResourceSetParamCommandPtr;

// -------------------------------------------------------------------------- //
// QnResourceSetParamsCommand
// -------------------------------------------------------------------------- //
class QnResourceSetParamsCommand : public QnResourceCommand
{
public:
    QnResourceSetParamsCommand(const QnResourcePtr &res, const QnCameraAdvancedParamValueList &values):
        QnResourceCommand(res),
        m_values(values)
    {}

    virtual void beforeDisconnectFromResource(){}

    bool execute()
    {
        bool success = isConnectedToTheResource();

        QnCameraAdvancedParamValueList result;
        if (success) {
            for(const QnCameraAdvancedParamValue &value: m_values) {
                if (m_resource->setParamPhysical(value.id, value.value))
                    result << value;
                else
                    success = false;
            }
        }
        emit m_resource->asyncParamsSetDone(m_resource, result);
        return success;
    }

private:
    QnCameraAdvancedParamValueList m_values;
};
typedef QSharedPointer<QnResourceSetParamsCommand> QnResourceSetParamsCommandPtr;


#endif // ENABLE_DATA_PROVIDERS

// -------------------------------------------------------------------------- //
// QnResource
// -------------------------------------------------------------------------- //
QnResource::QnResource(): 
    QObject(),
    m_mutex(QnMutex::Recursive),
    m_initMutex(QnMutex::Recursive),
    m_resourcePool(NULL),
    m_flags(0),
    m_initialized(false),
    m_lastInitTime(0),
    m_prevInitializationResult(CameraDiagnostics::ErrorCode::unknown),
    m_lastMediaIssue(CameraDiagnostics::NoErrorResult()),
    m_removedFromPool(false),
    m_initInProgress(false)
{
}

QnResource::~QnResource()
{
    disconnectAllConsumers();
}

QnResourcePool *QnResource::resourcePool() const 
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);

    return m_resourcePool;
}

void QnResource::setResourcePool(QnResourcePool *resourcePool) 
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);

    m_resourcePool = resourcePool;
}

QnResourcePtr QnResource::toSharedPointer() const
{
    return QnFromThisToShared<QnResource>::toSharedPointer();
}

void QnResource::afterUpdateInner(const QSet<QByteArray>& modifiedFields)
{
    emitModificationSignals( modifiedFields );
}

bool QnResource::emitDynamicSignal(const char *signal, void **arguments)
{
    QByteArray theSignal = QMetaObject::normalizedSignature(signal);
    int signalId = metaObject()->indexOfSignal(theSignal);
    if (signalId == -1)
        return false;
    metaObject()->activate(this, signalId, arguments);
    return true;
}

void QnResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) 
{
    Q_ASSERT(getId() == other->getId() || getUniqueId() == other->getUniqueId()); // unique id MUST be the same

    m_typeId = other->m_typeId;
    m_lastDiscoveredTime = other->m_lastDiscoveredTime;

    m_tags = other->m_tags;

    if (m_url != other->m_url) {
        m_url = other->m_url;
        modifiedFields << "urlChanged";
    }

    if (m_flags != other->m_flags) {
        m_flags = other->m_flags;    
        modifiedFields << "flagsChanged";
    }

    if (m_name != other->m_name) {
        m_name = other->m_name;
        modifiedFields << "nameChanged";
    }

    if (m_parentId != other->m_parentId) {
        m_parentId = other->m_parentId;
        modifiedFields << "parentIdChanged";
        if( m_initialized )
        {
            m_initialized = false;
            modifiedFields << "initializedChanged";
        }
    }

    m_locallySavedProperties = other->m_locallySavedProperties;
}

void QnResource::update(const QnResourcePtr& other, bool silenceMode) {
    /*
    Q_ASSERT_X(other->metaObject()->className() == this->metaObject()->className(),
        Q_FUNC_INFO,
        "Trying to update " + QByteArray(this->metaObject()->className()) + " with " + QByteArray(other->metaObject()->className()));
    */
    {
        SCOPED_MUTEX_LOCK( locker, &m_consumersMtx);
        for (QnResourceConsumer *consumer: m_consumers)
            consumer->beforeUpdate();
    }

    QSet<QByteArray> modifiedFields;
    {
        QnMutex *m1 = &m_mutex, *m2 = &other->m_mutex;
        if(m1 > m2)
            std::swap(m1, m2);  //to maintain mutex lock order
        SCOPED_MUTEX_LOCK( mutexLocker1, m1); 
        SCOPED_MUTEX_LOCK( mutexLocker2, m2); 
        updateInner(other, modifiedFields);
    }

    silenceMode |= other->hasFlags(Qn::foreigner);
    //setStatus(other->m_status, silenceMode);
    afterUpdateInner(modifiedFields);

    {
        SCOPED_MUTEX_LOCK( lk, &m_mutex); 
        if( !m_id.isNull() && !m_locallySavedProperties.empty() )
        {
            std::map<QString, LocalPropertyValue> locallySavedProperties;
            std::swap( locallySavedProperties, m_locallySavedProperties );
            QnUuid id = m_id;
            lk.unlock();

            for( auto prop: locallySavedProperties )
            {
                if( propertyDictionary->setValue(
                        id,
                        prop.first,
                        prop.second.value,
                        prop.second.markDirty,
                        prop.second.replaceIfExists) )   //isModified?
                {
                    emitPropertyChanged(prop.first);
                }
            }
        }
    }

    //silently ignoring missing properties because of removeProperty method lack
    for (const ec2::ApiResourceParamData &param: other->getProperties())
        emitPropertyChanged(param.name);   //here "propertyChanged" will be called

    SCOPED_MUTEX_LOCK( locker, &m_consumersMtx);
    for (QnResourceConsumer *consumer: m_consumers)
        consumer->afterUpdate();
}

QnUuid QnResource::getParentId() const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_parentId;
}

void QnResource::setParentId(const QnUuid& parent)
{
    bool initializedChanged = false;
    QnUuid oldParentId;
    {
        SCOPED_MUTEX_LOCK( locker, &m_mutex);
        if (m_parentId == parent)
            return;
        oldParentId = m_parentId;
        m_parentId = parent;
        if (m_initialized) {
            m_initialized = false;
            initializedChanged = true;
        }
    }
    
    if (!oldParentId.isNull())
        emit parentIdChanged(toSharedPointer(this));

    if (initializedChanged)
        emit this->initializedChanged(toSharedPointer(this));
}


QString QnResource::getName() const
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    return m_name;
}

void QnResource::setName(const QString& name)
{
    {
        SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);

        if(m_name == name)
            return;

        m_name = name;
    }

    emit nameChanged(toSharedPointer(this));
}

Qn::ResourceFlags QnResource::flags() const
{
    //SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    return m_flags;
}

void QnResource::setFlags(Qn::ResourceFlags flags)
{
    {
        SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);

        if(m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::addFlags(Qn::ResourceFlags flags)
{
    {
        SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
        flags |= m_flags;
        if(m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::removeFlags(Qn::ResourceFlags flags)
{
    {
        SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
        flags = m_flags & ~flags;
        if(m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

QString QnResource::toString() const
{
    return getName() + QLatin1String("  ") + getUniqueId();
}

QString QnResource::toSearchString() const
{
    return toString();
}

QnResourcePtr QnResource::getParentResource() const
{
    QnUuid parentID;
    QnResourcePool* resourcePool = NULL;
    {
        SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
        parentID = getParentId();
        resourcePool = m_resourcePool;
    }

    if (resourcePool)
        return resourcePool->getResourceById(parentID);
    else
        return QnResourcePtr();
}

bool QnResource::hasParam(const QString &name) const
{
    QnResourceTypePtr resType = qnResTypePool->getResourceType(m_typeId); 
    if (!resType)
        return false;
    return resType->hasParam(name);
}

bool QnResource::getParamPhysical(const QString &id, QString &value) {
    Q_UNUSED(id)
    Q_UNUSED(value)
    return false;
}

bool QnResource::setParamPhysical(const QString &id, const QString &value) {
    Q_UNUSED(id)
    Q_UNUSED(value)
    return false;
}

#ifdef ENABLE_DATA_PROVIDERS

void QnResource::setParamPhysicalAsync(const QString& name, const QString& value) {
    QnResourceSetParamCommandPtr command(new QnResourceSetParamCommand(toSharedPointer(this), name, value));
    addCommandToProc(command);
}

void QnResource::getParamPhysicalAsync(const QString& name) {
    QnResourceGetParamCommandPtr command(new QnResourceGetParamCommand(toSharedPointer(this), name));
    addCommandToProc(command);
}

void QnResource::getParamsPhysicalAsync(const QSet<QString> &ids) {
    QnResourceGetParamsCommandPtr command(new QnResourceGetParamsCommand(toSharedPointer(this), ids));
    addCommandToProc(command);
}

void QnResource::setParamsPhysicalAsync(const QnCameraAdvancedParamValueList &values) {
    QnResourceSetParamsCommandPtr command(new QnResourceSetParamsCommand(toSharedPointer(this), values));
    addCommandToProc(command);
}

#endif

bool QnResource::unknownResource() const
{
    return getName().isEmpty();
}

QnUuid QnResource::getTypeId() const
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(const QnUuid &id)
{
    if (id.isNull()) {
        qWarning() << "NULL typeId is set to resource" << getName();
        return;
    }

    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    m_typeId = id;
}

void QnResource::setTypeByName(const QString& resTypeName)
{
    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName(resTypeName);
    if (resType)
        setTypeId(resType->getId());
}

Qn::ResourceStatus QnResource::getStatus() const
{
    return qnStatusDictionary->value(getId());
}

void QnResource::doStatusChanged(Qn::ResourceStatus oldStatus, Qn::ResourceStatus newStatus)
{
#ifdef QN_RESOURCE_DEBUG
    qDebug() << "Change status. oldValue=" << oldStatus << " new value=" << newStatus << " id=" << m_id << " name=" << getName();
#endif

    if (newStatus == Qn::Offline || newStatus == Qn::Unauthorized) 
    {
        if(m_initialized) {
            m_initialized = false;
            emit initializedChanged(toSharedPointer(this));
        }
    }

    if ((oldStatus == Qn::Offline || oldStatus == Qn::NotDefined) && newStatus == Qn::Online && !hasFlags(Qn::foreigner))
        init();

    emit statusChanged(toSharedPointer(this));
}

void QnResource::setStatus(Qn::ResourceStatus newStatus, bool silenceMode)
{
    if (newStatus == Qn::NotDefined)
        return;

    if (m_removedFromPool)
        return;



    QnUuid id = getId();
    Qn::ResourceStatus oldStatus = qnStatusDictionary->value(id);
    if (oldStatus == newStatus)
        return;
    qnStatusDictionary->setValue(id, newStatus);
    if (!silenceMode)
        doStatusChanged(oldStatus, newStatus);
}

QDateTime QnResource::getLastDiscoveredTime() const
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    return m_lastDiscoveredTime;
}

void QnResource::setLastDiscoveredTime(const QDateTime &time)
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    m_lastDiscoveredTime = time;
}

QnUuid QnResource::getId() const
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    return m_id;
}

void QnResource::setId(const QnUuid& id) {
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);

    if(m_id == id)
        return;

    //QnUuid oldId = m_id;
    m_id = id;

    std::map<QString, LocalPropertyValue> locallySavedProperties;
    std::swap( locallySavedProperties, m_locallySavedProperties );
    mutexLocker.unlock();

    for( auto prop: locallySavedProperties )
    {
        if( propertyDictionary->setValue(
                id,
                prop.first,
                prop.second.value,
                prop.second.markDirty,
                prop.second.replaceIfExists) )   //isModified?
        {
            emitPropertyChanged(prop.first);
        }
    }
}

QString QnResource::getUrl() const
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    return m_url;
}

void QnResource::setUrl(const QString &url)
{
    {
        SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
        if(m_url == url)
            return;
        m_url = url;
    }
    emit urlChanged(toSharedPointer(this));
}

void QnResource::addTag(const QString& tag)
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    if (!m_tags.contains(tag))
        m_tags.push_back(tag);
}

void QnResource::setTags(const QStringList& tags)
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    m_tags = tags;
}

void QnResource::removeTag(const QString& tag)
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    m_tags.removeAll(tag);
}

bool QnResource::hasTag(const QString& tag) const
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    return m_tags.contains(tag);
}

QStringList QnResource::getTags() const
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    return m_tags;
}

void QnResource::addConsumer(QnResourceConsumer *consumer)
{
    SCOPED_MUTEX_LOCK( locker, &m_consumersMtx);

    if(m_consumers.contains(consumer)) {
        qnWarning("Given resource consumer '%1' is already associated with this resource.", typeid(*consumer).name());
        return;
    }

    m_consumers.insert(consumer);
}

void QnResource::removeConsumer(QnResourceConsumer *consumer)
{
    SCOPED_MUTEX_LOCK( locker, &m_consumersMtx);

    m_consumers.remove(consumer);
}

bool QnResource::hasConsumer(QnResourceConsumer *consumer) const
{
    SCOPED_MUTEX_LOCK( locker, &m_consumersMtx);
    return m_consumers.contains(consumer);
}

#ifdef ENABLE_DATA_PROVIDERS
bool QnResource::hasUnprocessedCommands() const
{
    SCOPED_MUTEX_LOCK( locker, &m_consumersMtx);
    for(QnResourceConsumer* consumer: m_consumers)
    {
        if (dynamic_cast<QnResourceCommand*>(consumer))
            return true;
    }

    return false;
}
#endif

void QnResource::disconnectAllConsumers()
{
    SCOPED_MUTEX_LOCK( locker, &m_consumersMtx);

    for (QnResourceConsumer *consumer: m_consumers)
        consumer->beforeDisconnectFromResource();

    for (QnResourceConsumer *consumer: m_consumers)
        consumer->disconnectFromResource();

    m_consumers.clear();
}

#ifdef ENABLE_DATA_PROVIDERS
QnAbstractStreamDataProvider* QnResource::createDataProvider(Qn::ConnectionRole role)
{
    QnAbstractStreamDataProvider* dataProvider = createDataProviderInternal(role);

    if(dataProvider != NULL && dataProvider->getResource() != this)
        qnCritical("createDataProviderInternal() returned a data provider that is not associated with current resource."); /* This may cause hard to debug problems. */

    return dataProvider;
}

QnAbstractStreamDataProvider *QnResource::createDataProviderInternal(Qn::ConnectionRole)
{
    return NULL;
}
#endif

QnAbstractPtzController *QnResource::createPtzController() {
    QnAbstractPtzController *result = createPtzControllerInternal();
    if(!result)
        return result;

    /* Do some sanity checking. */
    Qn::PtzCapabilities capabilities = result->getCapabilities();
    if((capabilities & Qn::LogicalPositioningPtzCapability) && !(capabilities & Qn::AbsolutePtzCapabilities))
        qnCritical("Logical position space capability is defined for a PTZ controller that does not support absolute movement.");
    if((capabilities & Qn::DevicePositioningPtzCapability) && !(capabilities & Qn::AbsolutePtzCapabilities))
        qnCritical("Device position space capability is defined for a PTZ controller that does not support absolute movement.");
    
    return result;
}

QnAbstractPtzController *QnResource::createPtzControllerInternal() {
    return NULL;
}

void QnResource::initializationDone()
{
}

bool QnResource::hasProperty(const QString &key) const {
    return propertyDictionary->hasProperty(getId(), key);
}

QString QnResource::getProperty(const QString &key) const {
    //SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    //return m_propertyByKey.value(key, defaultValue);
    QString value;
    {
        SCOPED_MUTEX_LOCK( lk, &m_mutex );
        if( m_id.isNull() ) {
            auto itr =  m_locallySavedProperties.find(key);
            if (itr != m_locallySavedProperties.end())
                value = itr->second.value;
        }
        else {
            value = propertyDictionary->value(getId(), key);
        }
    }

    if (value.isNull()) {
        // find default value in resourceType
        QnResourceTypePtr resType = qnResTypePool->getResourceType(m_typeId); 
        if (resType)
            return resType->defaultValue(key);
    }
    return value;
}

void QnResource::setProperty(const QString &key, const QString &value, bool markDirty, bool replaceIfExists) 
{
    {
        SCOPED_MUTEX_LOCK( lk, &m_mutex );
        if( m_id.isNull() )
        {
            //saving property to some internal dictionary. Will apply to global dictionary when id is known
            m_locallySavedProperties[key] = LocalPropertyValue( value, markDirty, replaceIfExists );
            return;
        }
    }

    Q_ASSERT(!getId().isNull());
    bool isModified = propertyDictionary->setValue(getId(), key, value, markDirty, replaceIfExists);
    if (isModified)
        emitPropertyChanged(key);
}

void QnResource::emitPropertyChanged(const QString& key)
{
    if(key == lit("ptzCapabilities"))
        emit ptzCapabilitiesChanged(::toSharedPointer(this));
    else if(key == Qn::VIDEO_LAYOUT_PARAM_NAME)
        emit videoLayoutChanged(::toSharedPointer(this));

    emit propertyChanged(toSharedPointer(this), key);
}

void QnResource::setProperty(const QString &key, const QVariant& value, bool markDirty, bool replaceIfExists)
{
    return setProperty(key, value.toString(), markDirty, replaceIfExists);
}

ec2::ApiResourceParamDataList QnResource::getProperties() const {
    return propertyDictionary->allProperties(getId());
}

void QnResource::emitModificationSignals( const QSet<QByteArray>& modifiedFields )
{
    emit resourceChanged(toSharedPointer(this));
    //modifiedFields << "resourceChanged";

    const QnResourcePtr & _t1 = toSharedPointer(this);
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    for(const QByteArray& signalName: modifiedFields)
        emitDynamicSignal((signalName + QByteArray("(QnResourcePtr)")).data(), _a);
}

QnInitResPool* QnResource::initAsyncPoolInstance()
{
    return &m_initAsyncPool;
}

// -----------------------------------------------------------------------------

#ifdef ENABLE_DATA_PROVIDERS
Q_GLOBAL_STATIC(QnResourceCommandProcessor, QnResourceCommandProcessor_instance)

void QnResource::startCommandProc()
{
    QnResourceCommandProcessor_instance()->start();
}

void QnResource::stopCommandProc()
{
    QnResourceCommandProcessor_instance()->stop();
}

void QnResource::addCommandToProc(const QnResourceCommandPtr& command)
{
    QnResourceCommandProcessor_instance()->putData(command);
}

int QnResource::commandProcQueueSize()
{
    return QnResourceCommandProcessor_instance()->queueSize();
}
#endif // ENABLE_DATA_PROVIDERS

bool QnResource::init()
{
    if(m_appStopping)
        return false;

    {
        SCOPED_MUTEX_LOCK( lock, &m_initMutex);
        if(m_initialized)
            return true; /* Nothing to do. */
        if (m_initInProgress)
            return false; /* Skip request if init is already running. */
        m_initInProgress = true;
    }

    CameraDiagnostics::Result initResult = initInternal();
    m_initMutex.lock();
    m_initInProgress = false;
    m_initialized = initResult.errorCode == CameraDiagnostics::ErrorCode::noError;
    {
        SCOPED_MUTEX_LOCK( lk, &m_mutex );
        m_prevInitializationResult = initResult;
    }
    m_initializationAttemptCount.fetchAndAddOrdered(1);
    
    bool changed = m_initialized;
    if(m_initialized) {
        initializationDone();
    } else if (getStatus() == Qn::Online || getStatus() == Qn::Recording) {
        setStatus(Qn::Offline);
    }

    m_initMutex.unlock();

    if(changed)
        emit initializedChanged(toSharedPointer(this));

    return true;
}

void QnResource::setLastMediaIssue(const CameraDiagnostics::Result& issue)
{
    SCOPED_MUTEX_LOCK( lk, &m_mutex );
    m_lastMediaIssue = issue;
}

CameraDiagnostics::Result QnResource::getLastMediaIssue() const
{
    SCOPED_MUTEX_LOCK( lk, &m_mutex );
    return m_lastMediaIssue;
}

void QnResource::blockingInit()
{
    if( !init() )
    {
        //init is running in another thread, waiting for it to complete...
        SCOPED_MUTEX_LOCK( lk, &m_initMutex );
    }
}

void QnResource::initAndEmit()
{
    init();
}

class InitAsyncTask: public QRunnable
{
public:
    InitAsyncTask(QnResourcePtr resource): m_resource(resource) {}
    void run()
    {
        m_resource->initAndEmit();
    }
private:
    QnResourcePtr m_resource;
};


void QnResource::stopAsyncTasks()
{
    m_appStopping = true;
    m_initAsyncPool.waitForDone();
}

void QnResource::pleaseStopAsyncTasks()
{
    m_appStopping = true;
}

void QnResource::initAsync(bool optional)
{
    qint64 t = getUsecTimer();

    SCOPED_MUTEX_LOCK( lock, &m_initAsyncMutex);

    if (t - m_lastInitTime < MIN_INIT_INTERVAL)
        return; 

    if (m_appStopping)
        return;

    if (hasFlags(Qn::foreigner))
        return; // removed to other server

    InitAsyncTask *task = new InitAsyncTask(toSharedPointer(this));
    if (optional) {
        if (m_initAsyncPool.tryStart(task))
            m_lastInitTime = t;
        else
            delete task;
    }
    else {
        m_lastInitTime = t;
        lock.unlock();
        m_initAsyncPool.start(task);
    }
}

CameraDiagnostics::Result QnResource::prevInitializationResult() const
{
    SCOPED_MUTEX_LOCK( lk, &m_mutex );
    return m_prevInitializationResult;
}

int QnResource::initializationAttemptCount() const
{
    return m_initializationAttemptCount.load();
}

bool QnResource::isInitialized() const
{
    return m_initialized;
}

void QnResource::setUniqId(const QString& value)
{
    Q_UNUSED(value)
    Q_ASSERT_X(false, Q_FUNC_INFO, "Not implemented");
}

Qn::PtzCapabilities QnResource::getPtzCapabilities() const
{
    return Qn::PtzCapabilities(getProperty(lit("ptzCapabilities")).toInt());
}

bool QnResource::hasPtzCapabilities(Qn::PtzCapabilities capabilities) const
{
    return getPtzCapabilities() & capabilities;
}

void QnResource::setPtzCapabilities(Qn::PtzCapabilities capabilities) {
    if (hasParam(lit("ptzCapabilities")))
        setProperty(lit("ptzCapabilities"), static_cast<int>(capabilities));
}

void QnResource::setPtzCapability(Qn::PtzCapabilities capability, bool value) {
    setPtzCapabilities(value ? (getPtzCapabilities() | capability) : (getPtzCapabilities() & ~capability));
}

void QnResource::setRemovedFromPool(bool value)
{
    SCOPED_MUTEX_LOCK( mutexLocker, &m_mutex);
    m_removedFromPool = value;
}
