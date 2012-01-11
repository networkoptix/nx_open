#include "resource.h"

#include "core/dataprovider/abstract_streamdataprovider.h"
#include "core/resourcemanagment/resource_pool.h"

#include "file_resource.h"
#include "resource_command_consumer.h"
#include "resource_consumer.h"

#include <limits.h>

QnResource::QnResource()
    : QObject(),
      m_rwLock(QReadWriteLock::Recursive),
      m_flags(0),
      m_avalable(true),
      m_status(Offline)
{
}

QnResource::~QnResource()
{
    disconnectAllConsumers();
}

void QnResource::updateInner(const QnResource& other)
{
    m_flags = other.m_flags;
    m_name = other.m_name;
    m_resourceParamList = other.m_resourceParamList;
    m_id = other.m_id;
    m_parentId = other.m_parentId;
    m_typeId = other.m_typeId;
    m_lastDiscoveredTime = other.m_lastDiscoveredTime;
    m_tags = other.m_tags;
    m_avalable = other.m_avalable;
    m_url = other.m_url;
    m_status = other.m_status;
    m_streamParamList = other.m_streamParamList;
}

void QnResource::update(const QnResource& other)
{
    foreach (QnResourceConsumer* consumer, m_consumers)
        consumer->beforeUpdate();

    {
        QWriteLocker writeLocker(&m_rwLock);
        QReadLocker readLocker(&other.m_rwLock);

        updateInner(other);
    }

    foreach (QnResourceConsumer* consumer, m_consumers)
        consumer->afterUpdate();
}

void QnResource::deserialize(const QnResourceParameters& parameters)
{
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
        setStatus(parameters[QLatin1String("status")] == "A" ? QnResource::Online : QnResource::Offline);
}

QnId QnResource::getParentId() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_parentId;
}

void QnResource::setParentId(const QnId& parent)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_parentId = parent;
}


QString QnResource::getName() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_name;
}

void QnResource::setName(const QString& name)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_name = name;
}

unsigned long QnResource::flags() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_flags;
}

bool QnResource::checkFlag(unsigned long flag) const
{
    QReadLocker readLocker(&m_rwLock);
    return (m_flags & flag) == flag;
}

void QnResource::setFlags(unsigned long flags)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_flags = flags;
}

void QnResource::addFlag(unsigned long flag)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_flags |= flag;
}

void QnResource::removeFlag(unsigned long flag)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_flags &= ~flag;
}

bool QnResource::associatedWithFile() const
{
    return (flags() & (ARCHIVE | SINGLE_SHOT)) != 0;
}

QString QnResource::toString() const
{
    return getName() + QLatin1String("  ") + getUniqueId();
}

QString QnResource::toSearchString() const
{
    return toString();
}

QnResourcePtr QnResource::toSharedPointer() const
{
    QnResourcePtr res = qnResPool->getResourceById(getId());
    Q_ASSERT_X(res != 0, Q_FUNC_INFO, "Resource not found");
    return res;
}

QnParamList &QnResource::getResourceParamList() const
{
    QnId restypeid;
    {
        QReadLocker readLocker(&m_rwLock);
        if (!m_resourceParamList.isEmpty())
            return m_resourceParamList;
        restypeid = m_typeId;
    }

    QnResourceTypePtr resType = qnResTypePool->getResourceType(restypeid);
    if (!resType)
        return m_resourceParamList;

    QnParamList resourceParamList;

    // 1. read Q_PROPERTY params
    const QMetaObject *mObject = metaObject();
    for (int i = 0; i < mObject->propertyCount(); ++i) {
        const QMetaProperty mProperty = mObject->property(i);
        QnParamTypePtr paramType(new QnParamType());
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
        paramType->name = mProperty.name();
        paramType->ui = mProperty.isDesignable();
        paramType->isStatic = true;
        //resType->addParamType(paramType);
        QnParam newParam(paramType);
        resourceParamList.append(newParam);
    }

    // 2. read AppServer params
    foreach (QnParamTypePtr paramType, resType->paramTypeList()) {
        QnParam newParam(paramType);
        newParam.setValue(paramType->default_value);
        resourceParamList.append(newParam);
    }

    QWriteLocker writeLocker(&m_rwLock);
    if (m_resourceParamList.isEmpty())
        m_resourceParamList = resourceParamList;

    return m_resourceParamList;
}

bool QnResource::hasSuchParam(const QString &name) const
{
    return getResourceParamList().contains(name);
}

bool QnResource::getParamPhysical(const QString& /*name*/, QVariant& /*val*/)
{
    return false;
}

bool QnResource::setParamPhysical(const QString& /*name*/, const QVariant& /*val*/)
{
    return false;
}

bool QnResource::setSpecialParam(const QString& /*name*/, const QVariant& /*val*/, QnDomain /*domain*/)
{
    return false;
}

bool QnResource::getParam(const QString &name, QVariant &val, QnDomain domain)
{
    if (!hasSuchParam(name))
    {
        cl_log.log("getParam: requested param does not exist!", cl_logWARNING);
        return false;
    }

    const QnParam &param = getResourceParamList().value(name);
    if (domain == QnDomainMemory)
    {
        QReadLocker readLocker(&m_rwLock);
        val = param.value();
        if (param.isStatic())
            val = this->property(param.name().toUtf8()).toString();
        return true;
    }
    else if (domain == QnDomainPhysical)
    {
        if (!param.isStatic() && getParamPhysical(param.name(), val)) {
            emit onParameterChanged(param.name(), val);
            return true;
        }
    }
    else if (domain == QnDomainDatabase)
    {
        if (!param.isStatic())
            return true;
    }

    return false;
}

bool QnResource::setParam(const QString& name, const QVariant& val, QnDomain domain)
{
    if (!hasSuchParam(name))
    {
        cl_log.log("setParam: requested param does not exist!", cl_logWARNING);
        return false;
    }

    QnParam &param = getResourceParamList().value(name);
    
    if (param.isReadOnly())
    {
        cl_log.log("setParam: cannot set readonly param!", cl_logWARNING);
        return false;
    }
    

    if (domain == QnDomainPhysical)
    {
        if (param.isStatic())
            return false;
            
        if (!setParamPhysical(param.name(), val))
            return false;
    }


    //QnDomainMemory should changed anyway
    {
        QReadLocker readLocker(&m_rwLock);
        if (!param.setValue(val)) 
        {
            cl_log.log("cannot set such param!", cl_logWARNING);
            return false;

        }

        if (param.isStatic())
            setProperty(param.name().toUtf8(), val);
    }


    emit onParameterChanged(param.name(), val);
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

      void execute()
      {
            if (isConnectedToTheResource()) {
                QVariant val;
                if (m_resource->getParam(m_name, val, m_domain)) {
                    if (m_domain != QnDomainPhysical)
                        QMetaObject::invokeMethod(m_resource.data(), "onParameterChanged", Qt::QueuedConnection, Q_ARG(QString, m_name), Q_ARG(QVariant, val));
                }
            }
      }

private:
    QString m_name;
    QnDomain m_domain;
};

typedef QSharedPointer<QnResourceGetParamCommand> QnResourceGetParamCommandPtr;

void QnResource::getParamAsynch(const QString &name, QnDomain domain)
{
    QnResourceGetParamCommandPtr command(new QnResourceGetParamCommand(toSharedPointer(), name, domain));
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

      void execute()
      {
            if (isConnectedToTheResource())
                m_resource->setParam(m_name, m_val, m_domain);
      }

private:
    QString m_name;
    QVariant m_val;
    QnDomain m_domain;
};

typedef QSharedPointer<QnResourceSetParamCommand> QnResourceSetParamCommandPtr;

void QnResource::setParamAsynch(const QString& name, const QVariant& val, QnDomain domain)
{
    QnResourceSetParamCommandPtr command(new QnResourceSetParamCommand(toSharedPointer(), name, val, domain));
    addCommandToProc(command);
}

bool QnResource::unknownResource() const
{
    return getName().isEmpty();
}

QnId QnResource::getTypeId() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_typeId;
}

void QnResource::setTypeId(const QnId& id)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_typeId = id;
}

QnResource::Status QnResource::getStatus() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_status;
}

void QnResource::setStatus(QnResource::Status status)
{
    if (m_status == status) // if status did not changed => do nothing
        return;

    if (m_status == Offline && status == Online)
        beforeUse();

    Status old_status;
    {
        QWriteLocker writeLocker(&m_rwLock);
        old_status = m_status;
        m_status = status;
    }
    emit onStatusChanged(old_status, status);
}


QDateTime QnResource::getLastDiscoveredTime() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_lastDiscoveredTime;
}

void QnResource::setLastDiscoveredTime(const QDateTime &time)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_lastDiscoveredTime = time;
}



QnId QnResource::getId() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_id;
}
void QnResource::setId(const QnId& id) {
    QWriteLocker writeLocker(&m_rwLock);
    m_id = id;
}

QString QnResource::getUrl() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_url;
}

void QnResource::setUrl(const QString& value)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_url = value;
}

void QnResource::addTag(const QString& tag)
{
    QWriteLocker writeLocker(&m_rwLock);
    if (!m_tags.contains(tag))
        m_tags.push_back(tag);
}

void QnResource::setTags(const QStringList& tags)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_tags = tags;
}

void QnResource::removeTag(const QString& tag)
{
    QWriteLocker writeLocker(&m_rwLock);
    m_tags.removeAll(tag);
}

bool QnResource::hasTag(const QString& tag) const
{
    QReadLocker readLocker(&m_rwLock);
    return m_tags.contains(tag);
}

QStringList QnResource::tagList() const
{
    QReadLocker readLocker(&m_rwLock);
    return m_tags;
}

void QnResource::addConsumer(QnResourceConsumer* consumer)
{
    QMutexLocker locker(&m_consumersMtx);
    m_consumers.insert(consumer);
}

void QnResource::removeConsumer(QnResourceConsumer* consumer)
{
    QMutexLocker locker(&m_consumersMtx);
    m_consumers.remove(consumer);
}

bool QnResource::hasSuchConsumer(QnResourceConsumer* consumer) const
{
    QMutexLocker locker(&m_consumersMtx);
    return m_consumers.contains(consumer);
}

void QnResource::disconnectAllConsumers()
{
    QMutexLocker locker(&m_consumersMtx);

    foreach(QnResourceConsumer* con, m_consumers)
    {
        con->beforeDisconnectFromResource();
    }

    foreach(QnResourceConsumer* con, m_consumers)
    {
        con->disconnectFromResource();
    }

    m_consumers.clear();
}

QnAbstractStreamDataProvider* QnResource::createDataProvider(ConnectionRole role)
{
    QnAbstractStreamDataProvider* dataProvider = createDataProviderInternal(role);
    if (dataProvider)
        addConsumer(dataProvider);
    return dataProvider;
}

// -----------------------------------------------------------------------------
// Temporary until real ResourceFactory is implemented
Q_GLOBAL_STATIC(QnResourceCommandProcessor, commandProcessor)

void QnResource::startCommandProc()
{
    commandProcessor()->start();
}

void QnResource::stopCommandProc()
{
    commandProcessor()->stop();
}

void QnResource::addCommandToProc(QnAbstractDataPacketPtr data)
{
    commandProcessor()->putData(data);
}

int QnResource::commandProcQueSize()
{
    return commandProcessor()->queueSize();
}

bool QnResource::commandProchasSuchDeviceInQueue(QnResourcePtr res)
{
    return commandProcessor()->hasSuchResourceInQueue(res);
}
