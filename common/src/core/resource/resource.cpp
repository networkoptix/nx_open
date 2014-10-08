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
    QnResourceGetParamCommand(QnResourcePtr res, const QString& name):
        QnResourceCommand(res),
        m_name(name)
    {}

    virtual void beforeDisconnectFromResource(){}

    bool execute()
    {
        bool rez = false;
        QVariant val;
        if (isConnectedToTheResource()) {
            rez = m_resource->getParamPhysical(m_name, val);
        }
        emit m_resource->asyncParamGetDone(m_resource, m_name, val, rez);
        return rez;
    }

private:
    QString m_name;
};

typedef QSharedPointer<QnResourceGetParamCommand> QnResourceGetParamCommandPtr;
#endif // ENABLE_DATA_PROVIDERS

// -------------------------------------------------------------------------- //
// QnResourceSetParamCommand
// -------------------------------------------------------------------------- //
#ifdef ENABLE_DATA_PROVIDERS
class QnResourceSetParamCommand : public QnResourceCommand
{
public:
    QnResourceSetParamCommand(QnResourcePtr res, const QString& name, const QVariant& val):
        QnResourceCommand(res),
        m_name(name),
        m_val(val)
    {}

    virtual void beforeDisconnectFromResource(){}

    bool execute()
    {
        if (!isConnectedToTheResource())
            return false;

        bool rez = m_resource->setParamPhysical(m_name, m_val);
        emit m_resource->asyncParamSetDone(m_resource, m_name, m_val, rez);
        return rez;
    }

private:
    QString m_name;
    QVariant m_val;
};
typedef QSharedPointer<QnResourceSetParamCommand> QnResourceSetParamCommandPtr;


#endif // ENABLE_DATA_PROVIDERS

// -------------------------------------------------------------------------- //
// QnResource
// -------------------------------------------------------------------------- //
QnResource::QnResource(): 
    QObject(),
    m_mutex(QMutex::Recursive),
    m_initMutex(QMutex::Recursive),
    m_resourcePool(NULL),
    m_flags(0),
    m_status(Qn::NotDefined),
    m_initialized(false),
    m_lastInitTime(0),
    m_prevInitializationResult(CameraDiagnostics::ErrorCode::unknown),
    m_lastMediaIssue(CameraDiagnostics::NoErrorResult())
{
    m_lastStatusUpdateTime = QDateTime::fromMSecsSinceEpoch(0);
}

QnResource::~QnResource()
{
    disconnectAllConsumers();
}

QnResourcePool *QnResource::resourcePool() const 
{
    QMutexLocker mutexLocker(&m_mutex);

    return m_resourcePool;
}

void QnResource::setResourcePool(QnResourcePool *resourcePool) 
{
    QMutexLocker mutexLocker(&m_mutex);

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
    }
}

void QnResource::update(const QnResourcePtr& other, bool silenceMode)
{
    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->beforeUpdate();
    QSet<QByteArray> modifiedFields;
    {
        QMutex *m1 = &m_mutex, *m2 = &other->m_mutex;
        if(m1 > m2)
            std::swap(m1, m2);
        QMutexLocker mutexLocker1(m1); 
        QMutexLocker mutexLocker2(m2); 
        updateInner(other, modifiedFields);
    }

    silenceMode |= other->hasFlags(Qn::foreigner);
    setStatus(other->m_status, silenceMode);
    afterUpdateInner(modifiedFields);

    //silently ignoring missing properties because of removeProperty method lack
    for (const ec2::ApiResourceParamData &param: other->getProperties())
        emitPropertyChanged(param.name);   //here "propertyChanged" will be called
    
    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->afterUpdate();
}

QnUuid QnResource::getParentId() const
{
    QMutexLocker locker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(QnUuid parent)
{
    bool initializedChanged = false;
    QnUuid oldParentId;
    {
        QMutexLocker locker(&m_mutex);
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
    QMutexLocker mutexLocker(&m_mutex);
    return m_name;
}

void QnResource::setName(const QString& name)
{
    {
        QMutexLocker mutexLocker(&m_mutex);

        if(m_name == name)
            return;

        m_name = name;
    }

    emit nameChanged(toSharedPointer(this));
}

Qn::ResourceFlags QnResource::flags() const
{
    //QMutexLocker mutexLocker(&m_mutex);
    return m_flags;
}

void QnResource::setFlags(Qn::ResourceFlags flags)
{
    {
        QMutexLocker mutexLocker(&m_mutex);

        if(m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::addFlags(Qn::ResourceFlags flags)
{
    {
        QMutexLocker mutexLocker(&m_mutex);
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
        QMutexLocker mutexLocker(&m_mutex);
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
        QMutexLocker mutexLocker(&m_mutex);
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

bool QnResource::getParamPhysical(const QString&/* name*/, QVariant &/*val*/)
{
    return false;
}

bool QnResource::setParamPhysical(const QString &/*param*/, const QVariant &/*val*/)
{
    return false;
}

#ifdef ENABLE_DATA_PROVIDERS

void QnResource::setParamPhysicalAsync(const QString& name, const QVariant& val)
{
    QnResourceSetParamCommandPtr command(new QnResourceSetParamCommand(toSharedPointer(this), name, val));
    addCommandToProc(command);
}

void QnResource::getParamPhysicalAsync(const QString& name)
{
    QnResourceGetParamCommandPtr command(new QnResourceGetParamCommand(toSharedPointer(this), name));
    addCommandToProc(command);
}

#endif

bool QnResource::unknownResource() const
{
    return getName().isEmpty();
}

QnUuid QnResource::getTypeId() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(const QnUuid &id)
{
    if (id.isNull()) {
        qWarning() << "NULL typeId is set to resource" << getName();
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);
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
    QMutexLocker mutexLocker(&m_mutex);
    return m_status;
}

void QnResource::setStatus(Qn::ResourceStatus newStatus, bool silenceMode)
{
    if (newStatus == Qn::NotDefined)
        return;

    Qn::ResourceStatus oldStatus;
    {
        QMutexLocker mutexLocker(&m_mutex);
        oldStatus = m_status;

        //if (oldStatus == Unauthorized && newStatus == Offline)
        //    return; 

        m_status = newStatus;

        if (silenceMode)
            return;
    }

    if (oldStatus == newStatus)
        return;

#ifdef QN_RESOURCE_DEBUG
    qDebug() << "Change status. oldValue=" << oldStatus << " new value=" << newStatus << " id=" << m_id << " name=" << getName();
#endif

    if (newStatus == Qn::Offline || newStatus == Qn::Unauthorized) {
        if(m_initialized) {
            m_initialized = false;
            emit initializedChanged(toSharedPointer(this));
        }
    }

    if (oldStatus == Qn::Offline && newStatus == Qn::Online && !hasFlags(Qn::foreigner))
        init();


    //if (hasFlags(foreigner)) {
    //    qWarning() << "Status changed for foreign resource!";
    //}

    //Q_ASSERT_X(!hasFlags(foreigner), Q_FUNC_INFO, "Status changed for foreign resource!");

    emit statusChanged(toSharedPointer(this));

    QDateTime dt = qnSyncTime->currentDateTime();
    QMutexLocker mutexLocker(&m_mutex);
    m_lastStatusUpdateTime = dt;
}

QDateTime QnResource::getLastStatusUpdateTime() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_lastStatusUpdateTime;
}

QDateTime QnResource::getLastDiscoveredTime() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_lastDiscoveredTime;
}

void QnResource::setLastDiscoveredTime(const QDateTime &time)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_lastDiscoveredTime = time;
}

QnUuid QnResource::getId() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_id;
}

void QnResource::setId(const QnUuid& id) {
    QMutexLocker mutexLocker(&m_mutex);

    if(m_id == id)
        return;

    //QnUuid oldId = m_id;
    m_id = id;

    auto locallySavedProperties = std::move( m_locallySavedProperties );
    mutexLocker.unlock();

    //<key, <value, isDirty>>
    for( std::pair<QString, std::pair<QString, bool> > prop: locallySavedProperties )
    {
        if( propertyDictionary->setValue(id, prop.first, prop.second.first, prop.second.second) )   //isModified?
            emitPropertyChanged(prop.first);
    }
}

QString QnResource::getUrl() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_url;
}

void QnResource::setUrl(const QString &url)
{
    {
        QMutexLocker mutexLocker(&m_mutex);
        if(m_url == url)
            return;
        m_url = url;
    }
    emit urlChanged(toSharedPointer(this));
}

void QnResource::addTag(const QString& tag)
{
    QMutexLocker mutexLocker(&m_mutex);
    if (!m_tags.contains(tag))
        m_tags.push_back(tag);
}

void QnResource::setTags(const QStringList& tags)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_tags = tags;
}

void QnResource::removeTag(const QString& tag)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_tags.removeAll(tag);
}

bool QnResource::hasTag(const QString& tag) const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_tags.contains(tag);
}

QStringList QnResource::getTags() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_tags;
}

void QnResource::addConsumer(QnResourceConsumer *consumer)
{
    QMutexLocker locker(&m_consumersMtx);

    if(m_consumers.contains(consumer)) {
        qnWarning("Given resource consumer '%1' is already associated with this resource.", typeid(*consumer).name());
        return;
    }

    m_consumers.insert(consumer);
}

void QnResource::removeConsumer(QnResourceConsumer *consumer)
{
    QMutexLocker locker(&m_consumersMtx);

    m_consumers.remove(consumer);
}

bool QnResource::hasConsumer(QnResourceConsumer *consumer) const
{
    QMutexLocker locker(&m_consumersMtx);
    return m_consumers.contains(consumer);
}

#ifdef ENABLE_DATA_PROVIDERS
bool QnResource::hasUnprocessedCommands() const
{
    QMutexLocker locker(&m_consumersMtx);
    foreach(QnResourceConsumer* consumer, m_consumers)
    {
        if (dynamic_cast<QnResourceCommand*>(consumer))
            return true;
    }

    return false;
}
#endif

void QnResource::disconnectAllConsumers()
{
    QMutexLocker locker(&m_consumersMtx);

    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->beforeDisconnectFromResource();

    foreach (QnResourceConsumer *consumer, m_consumers)
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
    //QMutexLocker mutexLocker(&m_mutex);
    //return m_propertyByKey.value(key, defaultValue);
    QString value = propertyDictionary->value(getId(), key);
    if (value.isNull()) {
        // find default value in resourceType
        QnResourceTypePtr resType = qnResTypePool->getResourceType(m_typeId); 
        if (resType)
            return resType->defaultValue(key);
    }
    return value;
}

void QnResource::setProperty(const QString &key, const QString &value, bool markDirty) 
{
    {
        QMutexLocker lk( &m_mutex );
        if( m_id.isNull() )
        {
            //saving property to some internal dictionary. Will apply to global dictionary when id is known
            m_locallySavedProperties[key] = std::make_pair( value, markDirty );
            return;
        }
    }

    Q_ASSERT(!getId().isNull());
    bool isModified = propertyDictionary->setValue(getId(), key, value, markDirty);
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

void QnResource::setProperty(const QString &key, const QVariant& value)
{
    return setProperty(key, value.toString());
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
    foreach(const QByteArray& signalName, modifiedFields)
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

    if(!m_initMutex.tryLock())
        return false; /* Skip request if init is already running. */

    if(m_initialized) {
        m_initMutex.unlock();
        return true; /* Nothing to do. */
    }

    CameraDiagnostics::Result initResult = initInternal();
    m_initialized = initResult.errorCode == CameraDiagnostics::ErrorCode::noError;
    {
        QMutexLocker lk( &m_mutex );
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
    QMutexLocker lk( &m_mutex );
    m_lastMediaIssue = issue;
}

CameraDiagnostics::Result QnResource::getLastMediaIssue() const
{
    QMutexLocker lk( &m_mutex );
    return m_lastMediaIssue;
}

void QnResource::blockingInit()
{
    if( !init() )
    {
        //init is running in another thread, waiting for it to complete...
        QMutexLocker lk( &m_initMutex );
    }
}

void QnResource::initAndEmit()
{
    init();
    emit initAsyncFinished(toSharedPointer(this), isInitialized());
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

    QMutexLocker lock(&m_initAsyncMutex);

    if (t - m_lastInitTime < MIN_INIT_INTERVAL)
        return; 

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
    QMutexLocker lk( &m_mutex );
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
