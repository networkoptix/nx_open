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
    QnResourceGetParamCommand(QnResourcePtr res, const QString& name, QnDomain domain):
        QnResourceCommand(res),
        m_name(name),
        m_domain(domain)
    {}

    virtual void beforeDisconnectFromResource(){}

    bool execute()
    {
        if (!isConnectedToTheResource())
            return false;

        QVariant val;
        return m_resource->getParam(m_name, val, m_domain);
    }

private:
    QString m_name;
    QnDomain m_domain;
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
    QnResourceSetParamCommand(QnResourcePtr res, const QString& name, const QVariant& val, QnDomain domain):
        QnResourceCommand(res),
        m_name(name),
        m_val(val),
        m_domain(domain)
    {}

    virtual void beforeDisconnectFromResource(){}

    bool execute()
    {
        if (!isConnectedToTheResource())
            return false;

        return m_resource->setParam(m_name, m_val, m_domain);
    }

private:
    QString m_name;
    QVariant m_val;
    QnDomain m_domain;
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

void QnResource::afterUpdateInner(QSet<QByteArray>& modifiedFields)
{
    emit resourceChanged(toSharedPointer(this));
    //modifiedFields << "resourceChanged";

    const QnResourcePtr & _t1 = toSharedPointer(this);
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    foreach(const QByteArray& signalName, modifiedFields)
        emitDynamicSignal((signalName + QByteArray("(QnResourcePtr)")).data(), _a);
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

void QnResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) {
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

    QnParamList paramList = other->getResourceParamList();
    foreach(QnParam param, paramList.list())
    {
        if (param.domain() == QnDomainDatabase)
        {
            setParam(param.name(), param.value(), QnDomainDatabase);
        }
    }

    //silently ignoring missing properties because of removeProperty method lack
    for (const QnKvPair &param: other->getProperties()) {
        setProperty(param.name(), param.value());   //here "propertyChanged" will be called
    }

    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->afterUpdate();
}

QUuid QnResource::getParentId() const
{
    QMutexLocker locker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(QUuid parent)
{
    bool initializedChanged = false;
    QUuid oldParentId;
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
    QUuid parentID;
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


QnParamList QnResource::getResourceParamList() const
{
    QMutexLocker mutexLocker(&m_mutex);
    
    if (!m_resourceParamList.isEmpty())
        return m_resourceParamList;
    QUuid resTypeId = m_typeId;
    
    QnParamList resourceParamList;

    // 2. read AppServer params 
    if (QnResourceTypePtr resType = qnResTypePool->getResourceType(resTypeId)) 
    {
        Q_ASSERT(resType);

        const QList<QnParamTypePtr>& params = resType->paramTypeList();
        //Q_ASSERT(params.size() != 0);
        for (int i = 0; i < params.size(); ++i)
        {
            QnParam newParam(params[i], params[i]->default_value);
            resourceParamList.append(newParam);
        }
    }

    if (m_resourceParamList.isEmpty())
        m_resourceParamList = resourceParamList;

    return m_resourceParamList;
}

bool QnResource::hasParam(const QString &name) const
{
    return getResourceParamList().contains(name);
}

bool QnResource::getParamPhysical(const QnParam &/*param*/, QVariant &/*val*/)
{
    return false;
}

bool QnResource::setParamPhysical(const QnParam &/*param*/, const QVariant &/*val*/)
{
    return false;
}

bool QnResource::setSpecialParam(const QString& /*name*/, const QVariant& /*val*/, QnDomain /*domain*/)
{
    return false;
}

bool QnResource::getParam(const QString &name, QVariant &val, QnDomain domain) const
{
    const QnParamList& resourceParamList = getResourceParamList();
    QnParamList::const_iterator paramIter = resourceParamList.find( name );
    if ( paramIter == resourceParamList.cend() )
    {
        emit asyncParamGetDone(toSharedPointer(const_cast<QnResource*>(this)), name, QVariant(), false);
        return false;
    }

    const QnParam& param = paramIter.value();
    val = param.value();

    if (domain == QnDomainMemory)
    {
        emit asyncParamGetDone(toSharedPointer(const_cast<QnResource*>(this)), name, val, true);
        return true;
    }
    else if (domain == QnDomainPhysical)
    {
        QVariant newValue;
        if (param.isPhysical() && const_cast<QnResource*>(this)->getParamPhysical(param, newValue)) {
            if (val != newValue) {
                val = newValue;
                m_mutex.lock();
                //param.setValue(newValue);
                m_resourceParamList[name].setValue(newValue);
                m_mutex.unlock();
                const_cast<QnResource *>(this)->parameterValueChangedNotify(param); // TODO: wtf???
            }
            emit asyncParamGetDone(toSharedPointer(const_cast<QnResource*>(this)), name, newValue, true);
            return true;
        }
    }
    else if (domain == QnDomainDatabase)
    {
        if (param.isPhysical())
        {
            emit asyncParamGetDone(toSharedPointer(const_cast<QnResource*>(this)), name, val, true);
            return true;
        }
    }

    emit asyncParamGetDone(toSharedPointer(const_cast<QnResource*>(this)), name, QVariant(), false);
    return false;
}

void QnResource::parameterValueChangedNotify(const QnParam &param) {
    if(param.name() == lit("ptzCapabilities"))
        emit ptzCapabilitiesChanged(::toSharedPointer(this));
    else if(param.name() == lit("VideoLayout"))
        emit videoLayoutChanged(::toSharedPointer(this));

    emit parameterValueChanged(::toSharedPointer(this), param);
}

bool QnResource::setParam(const QString &name, const QVariant &val, QnDomain domain)
{
    if (setSpecialParam(name, val, domain))
    {
        emit asyncParamSetDone(toSharedPointer(this), name, val, true);
        return true;
    }

    const QnParamList& resourceParamList = getResourceParamList(); // paramList loaded once. No more changes, instead of param value. So, use mutex for value only
    if (!resourceParamList.contains(name))
    {
        qWarning() << "Can't set parameter. Parameter" << name << "does not exists for resource" << getName();
        emit asyncParamSetDone(toSharedPointer(this), name, val, false);
        return false;
    }


    m_mutex.lock();
    QnParam param = m_resourceParamList[name];
    if (param.isReadOnly())
    {
        NX_LOG("setParam: cannot set readonly param!", cl_logWARNING);
        m_mutex.unlock();
        emit asyncParamSetDone(toSharedPointer(this), name, val, false);
        return false;
    }
    param.setDomain(domain);

    QVariant oldValue = param.value();
    m_mutex.unlock();

    if (domain == QnDomainPhysical)
    {
        if (!param.isPhysical() || !setParamPhysical(param, val))
        {
            emit asyncParamSetDone(toSharedPointer(this), name, val, false);
            return false;
        }
    }

    //QnDomainMemory should be changed anyway
    {
        QMutexLocker locker(&m_mutex); // block paramList changing
        QnParam& param = m_resourceParamList[name];
        param.setDomain(domain);
        if (!param.setValue(val))
        {
            locker.unlock();
            NX_LOG( lit("cannot set such param %1!").arg(name), cl_logWARNING );
            emit asyncParamSetDone(toSharedPointer(this), name, val, false);
            return false;
        }
    }

    if (oldValue != val)
        parameterValueChangedNotify(param);

    emit asyncParamSetDone(toSharedPointer(this), name, val, true);
    return true;
}

#ifdef ENABLE_DATA_PROVIDERS
void QnResource::getParamAsync(const QString &name, QnDomain domain)
{
    QnResourceGetParamCommandPtr command(new QnResourceGetParamCommand(toSharedPointer(this), name, domain));
    addCommandToProc(command);
}

void QnResource::setParamAsync(const QString& name, const QVariant& val, QnDomain domain)
{
    QnResourceSetParamCommandPtr command(new QnResourceSetParamCommand(toSharedPointer(this), name, val, domain));
    addCommandToProc(command);
}
#endif // ENABLE_DATA_PROVIDERS

bool QnResource::unknownResource() const
{
    return getName().isEmpty();
}

QUuid QnResource::getTypeId() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(QUuid id)
{
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

QUuid QnResource::getId() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_id;
}

void QnResource::setId(const QUuid& id) {
    QMutexLocker mutexLocker(&m_mutex);

    if(m_id == id)
        return;

    //QUuid oldId = m_id;
    m_id = id;
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
    QMutexLocker mutexLocker(&m_mutex);
    return m_propertyByKey.contains(key);
}

QString QnResource::getProperty(const QString &key, const QString &defaultValue) const {
    QMutexLocker mutexLocker(&m_mutex);
    return m_propertyByKey.value(key, defaultValue);
}

void QnResource::setProperty(const QString &key, const QString &value) {
    {
        QMutexLocker mutexLocker(&m_mutex);
        if (m_propertyByKey.contains(key) && m_propertyByKey.value(key) == value)
            return;
        m_propertyByKey[key] = value;
    }
    emit propertyChanged(toSharedPointer(this), key);
}

QnKvPairList QnResource::getProperties() const {
    QMutexLocker mutexLocker(&m_mutex);
    
    QnKvPairList result;
    for(auto pos = m_propertyByKey.begin(); pos != m_propertyByKey.end(); pos++)
        result.push_back(QnKvPair(pos.key(), pos.value()));
    return result;
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
    QVariant mediaVariant;
    QnResource* thisCasted = const_cast<QnResource*>(this);
    thisCasted->getParam(QLatin1String("ptzCapabilities"), mediaVariant, QnDomainMemory);
    return static_cast<Qn::PtzCapabilities>(mediaVariant.toInt());
}

bool QnResource::hasPtzCapabilities(Qn::PtzCapabilities capabilities) const
{
    return getPtzCapabilities() & capabilities;
}

void QnResource::setPtzCapabilities(Qn::PtzCapabilities capabilities) {
    if (hasParam(lit("ptzCapabilities")))
        setParam(lit("ptzCapabilities"), static_cast<int>(capabilities), QnDomainDatabase);
}

void QnResource::setPtzCapability(Qn::PtzCapabilities capability, bool value) {
    setPtzCapabilities(value ? (getPtzCapabilities() | capability) : (getPtzCapabilities() & ~capability));
}
