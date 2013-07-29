#include "resource.h"

#include <climits>

#include <typeinfo>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QRunnable>

#include "utils/common/warnings.h"

#include "core/dataprovider/abstract_streamdataprovider.h"
#include "core/resource_managment/resource_pool.h"

#include "resource_command_processor.h"
#include "resource_consumer.h"
#include "resource_property.h"

#include "utils/common/synctime.h"
#include "utils/common/util.h"

bool QnResource::m_appStopping = false;
QnInitResPool QnResource::m_initAsyncPool;


static const qint64 MIN_INIT_INTERVAL = 1000000ll * 30;

QnResource::QnResource(): 
    QObject(),
    m_mutex(QMutex::Recursive),
    m_initMutex(QMutex::Recursive),
    m_resourcePool(NULL),
    m_flags(0),
    m_disabled(false),
    m_status(Offline),
    m_initialized(false),
    m_lastInitTime(0)
{
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

void QnResource::setGuid(const QString& guid)
{
    QMutexLocker mutexLocker(&m_mutex);

    m_guid = guid;
}

QString QnResource::getGuid() const
{
    QMutexLocker mutexLocker(&m_mutex);

    return m_guid;
}

QnResourcePtr QnResource::toSharedPointer() const
{
    return QnFromThisToShared<QnResource>::toSharedPointer();
}

void QnResource::updateInner(QnResourcePtr other)
{
    Q_ASSERT(getUniqueId() == other->getUniqueId()); // unique id MUST be the same

    m_id = other->m_id; //TODO: #Elric this is WRONG!!!!!!!!!11111111
    m_typeId = other->m_typeId;
    m_lastDiscoveredTime = other->m_lastDiscoveredTime;
    
    m_tags = other->m_tags;
    m_url = other->m_url;
    m_flags = other->m_flags;
    m_name = other->m_name;
    m_parentId = other->m_parentId;
}

void QnResource::update(QnResourcePtr other, bool silenceMode)
{
    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->beforeUpdate();

    {
        QMutex *m1 = &m_mutex, *m2 = &other->m_mutex;
        if(m1 > m2)
            std::swap(m1, m2);
        QMutexLocker mutexLocker1(m1); 
        QMutexLocker mutexLocker2(m2); 
        updateInner(other); 
    }

    silenceMode |= other->hasFlags(QnResource::foreigner);
    setStatus(other->m_status, silenceMode);
    setDisabled(other->m_disabled);
    emit resourceChanged(toSharedPointer(this));

    QnParamList paramList = other->getResourceParamList();
    foreach(QnParam param, paramList.list())
    {
        if (param.domain() == QnDomainDatabase)
        {
            setParam(param.name(), param.value(), QnDomainDatabase);
        }
    }

    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->afterUpdate();
}

void QnResource::deserialize(const QnResourceParameters& parameters)
{
    bool signalsBlocked = blockSignals(true);

    QMutexLocker locker(&m_mutex);

    if (parameters.contains(QLatin1String("id")))
        setId(parameters[QLatin1String("id")]);

    if (parameters.contains(QLatin1String("typeId")))
        setTypeId(parameters[QLatin1String("typeId")]);

    if (parameters.contains(QLatin1String("parentId")))
        setParentId(parameters[QLatin1String("parentId")]);

    if (parameters.contains(QLatin1String("name")))
        setName(parameters[QLatin1String("name")]);

    if (parameters.contains(QLatin1String("url")))
        setUrl(parameters[QLatin1String("url")]);

    if (parameters.contains(QLatin1String("status")))
        m_status = (QnResource::Status)parameters[QLatin1String("status")].toInt();

    if (parameters.contains(QLatin1String("disabled")))
        m_disabled = parameters[QLatin1String("disabled")].toInt();

    blockSignals(signalsBlocked);
}

QnId QnResource::getParentId() const
{
    QMutexLocker locker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(QnId parent)
{
    {
        QMutexLocker locker(&m_mutex);
        m_parentId = parent;
    }
    
    emit parentIdChanged(toSharedPointer(this));
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

QnResource::Flags QnResource::flags() const
{
    //QMutexLocker mutexLocker(&m_mutex);
    return m_flags;
}

void QnResource::setFlags(Flags flags)
{
    {
        QMutexLocker mutexLocker(&m_mutex);

        if(m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::addFlags(Flags flags)
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

void QnResource::removeFlags(Flags flags)
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
    QnId parentID;
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
    QnId resTypeId = m_typeId;
    
    QnParamList resourceParamList;

    /*
    // 1. read Q_PROPERTY params
    const QMetaObject *mObject = metaObject();
    for (int i = 1; i < mObject->propertyCount(); ++i) { // from 1 to skip `objectName`
        const QMetaProperty mProperty = mObject->property(i);
        QByteArray propertyName(mProperty.name());

        QnParamTypePtr paramType(new QnParamType);
        paramType->name = QString::fromLatin1(propertyName.constData(), propertyName.size());
        paramType->isReadOnly = !mProperty.isWritable();
        paramType->ui = mProperty.isDesignable();
        paramType->isPhysical = false;
        if (mProperty.isEnumType()) {
            paramType->type = QnParamType::Enumeration;
            const QMetaEnum mEnumerator = mProperty.enumerator();
            for (int i = 0; i < mEnumerator.keyCount(); ++i) {
                paramType->ui_possible_values.append(mEnumerator.key(i));
                paramType->possible_values.append(mEnumerator.value(i));
            }
        } else {
            switch (mProperty.userType()) {
            case QVariant::Bool:
                paramType->type = QnParamType::Boolen;
                break;
            case QVariant::UInt:
                paramType->type = QnParamType::MinMaxStep;
                paramType->min_val = 0;
                paramType->max_val = UINT_MAX;
                break;
            case QVariant::Int:
                paramType->type = QnParamType::MinMaxStep;
                paramType->min_val = INT_MIN;
                paramType->max_val = INT_MAX;
                break;
            case QVariant::ULongLong:
                paramType->type = QnParamType::MinMaxStep;
                paramType->min_val = 0;
                paramType->max_val = ULLONG_MAX;
                break;
            case QVariant::LongLong:
                paramType->type = QnParamType::MinMaxStep;
                paramType->min_val = LLONG_MIN;
                paramType->max_val = LLONG_MAX;
                break;
            default:
                paramType->type = QnParamType::Value;
                break;
            }
        }
        paramType->setDefVal(QVariant(mProperty.userType(), (void *)0));
        for (int j = 0; j < mObject->classInfoCount(); ++j) {
            QMetaClassInfo classInfo = mObject->classInfo(j);
            if (propertyName == classInfo.name())
                paramType->description = QCoreApplication::translate("QnResource", classInfo.value());
            else if (propertyName + "_group" == classInfo.name())
                paramType->group = QString::fromLatin1(classInfo.value());
            else if (propertyName + "_subgroup" == classInfo.name())
                paramType->subgroup = QString::fromLatin1(classInfo.value());
        }

        QnParam newParam(paramType, mProperty.read(this));
        resourceParamList.append(newParam);
    }
    */

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

bool QnResource::getParam(const QString &name, QVariant &val, QnDomain domain)
{
    getResourceParamList();
    if (!m_resourceParamList.contains(name))
    {
        emit asyncParamGetDone(toSharedPointer(this), name, QVariant(), false);
        return false;
    }

    //m_mutex.lock();
    //QnParam &param = m_resourceParamList[name];
    //val = param.value();
    //m_mutex.unlock();
    m_mutex.lock();
    QnParam param = m_resourceParamList[name];
    val = param.value();
    m_mutex.unlock();

    if (domain == QnDomainMemory)
    {
        emit asyncParamGetDone(toSharedPointer(this), name, val, true);
        return true;
    }
    else if (domain == QnDomainPhysical)
    {
        QVariant newValue;
        if (param.isPhysical() && getParamPhysical(param, newValue)) {
            if (val != newValue) {
                val = newValue;
                m_mutex.lock();
                //param.setValue(newValue);
                m_resourceParamList[name].setValue(newValue);
                m_mutex.unlock();
                emit parameterValueChanged(::toSharedPointer(this), param);
            }
            emit asyncParamGetDone(toSharedPointer(this), name, newValue, true);
            return true;
        }
    }
    else if (domain == QnDomainDatabase)
    {
        if (param.isPhysical())
        {
            emit asyncParamGetDone(toSharedPointer(this), name, val, true);
            return true;
        }
    }

    emit asyncParamGetDone(toSharedPointer(this), name, QVariant(), false);
    return false;
}

bool QnResource::setParam(const QString &name, const QVariant &val, QnDomain domain)
{
    if (setSpecialParam(name, val, domain))
    {
        emit asyncParamSetDone(toSharedPointer(this), name, val, true);
        return true;
    }

    getResourceParamList(); // paramList loaded once. No more changes, instead of param value. So, use mutex for value only
    if (!m_resourceParamList.contains(name))
    {
        qWarning() << "Can't set parameter. Parameter" << name << "does not exists for resource" << getName();
        emit asyncParamSetDone(toSharedPointer(this), name, val, false);
        return false;
    }

    m_mutex.lock();
    QnParam param = m_resourceParamList[name];
    if (param.isReadOnly())
    {
        cl_log.log("setParam: cannot set readonly param!", cl_logWARNING);
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

    //QnDomainMemory should changed anyway
    {
        QMutexLocker locker(&m_mutex); // block paramList changing
        m_resourceParamList[name].setDomain(domain);
        if (!m_resourceParamList[name].setValue(val))
        {
            locker.unlock();
            cl_log.log("cannot set such param!", cl_logWARNING);
            emit asyncParamSetDone(toSharedPointer(this), name, val, false);
            return false;
        }
    }

    if (oldValue != val)
        emit parameterValueChanged(::toSharedPointer(this), param);

    emit asyncParamSetDone(toSharedPointer(this), name, val, true);
    return true;
}

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

void QnResource::getParamAsync(const QString &name, QnDomain domain)
{
    QnResourceGetParamCommandPtr command(new QnResourceGetParamCommand(toSharedPointer(this), name, domain));
    addCommandToProc(command);
}

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

void QnResource::setParamAsync(const QString& name, const QVariant& val, QnDomain domain)
{
    QnResourceSetParamCommandPtr command(new QnResourceSetParamCommand(toSharedPointer(this), name, val, domain));
    addCommandToProc(command);
}

bool QnResource::unknownResource() const
{
    return getName().isEmpty();
}

QnId QnResource::getTypeId() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(QnId id)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_typeId = id;
}

QnResource::Status QnResource::getStatus() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_status;
}

void QnResource::setStatus(QnResource::Status newStatus, bool silenceMode)
{
    Status oldStatus;
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

    if (newStatus == Offline || newStatus == Unauthorized)
        m_initialized = false;

    if (oldStatus == Offline && newStatus == Online && !m_disabled)
        init();


    Q_ASSERT_X(!hasFlags(foreigner), Q_FUNC_INFO, "Status changed for foreign resource!");

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

QnId QnResource::getId() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_id;
}

void QnResource::setId(QnId id) {
    QMutexLocker mutexLocker(&m_mutex);

    if(m_id == id)
        return;

    //QnId oldId = m_id;
    m_id = id;
}

QString QnResource::getUrl() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_url;
}

void QnResource::setUrl(const QString &url)
{
    QMutexLocker mutexLocker(&m_mutex);

    if(m_url == url)
        return;

    QString oldUrl = m_url;
    m_url = url;

    mutexLocker.unlock();

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


void QnResource::disconnectAllConsumers()
{
    QMutexLocker locker(&m_consumersMtx);

    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->beforeDisconnectFromResource();

    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->disconnectFromResource();

    m_consumers.clear();
}

QnAbstractStreamDataProvider* QnResource::createDataProvider(ConnectionRole role)
{
    QnAbstractStreamDataProvider* dataProvider = createDataProviderInternal(role);

    if(dataProvider != NULL && dataProvider->getResource() != this)
        qnCritical("createDataProviderInternal() returned a data provider that is not associated with current resource."); /* This may cause hard to debug problems. */

    return dataProvider;
}

QnAbstractStreamDataProvider *QnResource::createDataProviderInternal(ConnectionRole role)
{
    Q_UNUSED(role)
    return 0;
}

void QnResource::initializationDone()
{
}

// -----------------------------------------------------------------------------
// Temporary until real ResourceFactory is implemented
Q_GLOBAL_STATIC(QnResourceCommandProcessor, QnResourceCommandProcessor_instance)

void QnResource::startCommandProc()
{
    QnResourceCommandProcessor_instance()->start();
}

void QnResource::stopCommandProc()
{
    QnResourceCommandProcessor_instance()->stop();
}

void QnResource::addCommandToProc(QnAbstractDataPacketPtr data)
{
    QnResourceCommandProcessor_instance()->putData(data);
}

int QnResource::commandProcQueueSize()
{
    return QnResourceCommandProcessor_instance()->queueSize();
}

bool QnResource::isDisabled() const
{
    //QMutexLocker mutexLocker(&m_mutex);

    return m_disabled;
}

void QnResource::setDisabled(bool disabled)
{
    if (m_disabled == disabled)
        return;

    bool oldDisabled = m_disabled;

    {
        QMutexLocker mutexLocker(&m_mutex);

        m_disabled = disabled;
        m_initialized = false;
    }

    if (oldDisabled != disabled)
        emit disabledChanged(toSharedPointer(this));
}

void QnResource::init()
{
    if (m_appStopping)
        return;

    if (!m_initMutex.tryLock())
        return; // if init already running, skip new request

    if (!m_initialized) 
    {
        m_initialized = initInternal();
        if( m_initialized )
            initializationDone();
        if (!m_initialized && (getStatus() == Online || getStatus() == Recording))
            setStatus(Offline);
    }
    m_initMutex.unlock();
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

bool QnResource::isInitialized() const
{
    return m_initialized;
}

QnAbstractPtzController* QnResource::getPtzController()
{
    return 0;
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
    return Qn::undeprecatePtzCapabilities(static_cast<Qn::PtzCapabilities>(mediaVariant.toInt()));
}

bool QnResource::hasPtzCapabilities(Qn::PtzCapabilities capabilities) const
{
    return getPtzCapabilities() & capabilities;
}

void QnResource::setPtzCapabilities(Qn::PtzCapabilities capabilities) {
    if (hasParam(lit("ptzCapabilities")))
        setParam(lit("ptzCapabilities"), static_cast<int>(capabilities), QnDomainDatabase);
    emit ptzCapabilitiesChanged(::toSharedPointer(this)); // TODO: #Elric we don't check whether they have actually changed. This better be fixed.
}

void QnResource::setPtzCapability(Qn::PtzCapabilities capability, bool value) {
    setPtzCapabilities(value ? (getPtzCapabilities() | capability) : (getPtzCapabilities() & ~capability));
}
