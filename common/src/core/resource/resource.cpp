#include "resource.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>

#include "utils/common/warnings.h"

#include "core/dataprovider/abstract_streamdataprovider.h"
#include "core/resourcemanagment/resource_pool.h"

#include "file_resource.h"
#include "resource_command_consumer.h"
#include "resource_consumer.h"

#include <limits.h>

static const char *property_descriptions[] = {
    QT_TRANSLATE_NOOP("QnResource", "URL")
};

QnResource::QnResource()
    : QObject(),
      m_mutex(QMutex::Recursive),
      m_flags(0),
      m_status(Offline)
{
}

QnResource::~QnResource()
{
    disconnectAllConsumers();
}

void QnResource::updateInner(const QnResource& other)
{
    m_id = other.m_id;
    m_parentId = other.m_parentId;
    m_typeId = other.m_typeId;
    m_flags = other.m_flags;
    m_name = other.m_name;
    m_lastDiscoveredTime = other.m_lastDiscoveredTime;
    m_tags = other.m_tags;
    m_url = other.m_url;
}

void QnResource::update(const QnResource& other)
{
    foreach (QnResourceConsumer *consumer, m_consumers)
        consumer->beforeUpdate();

    {
        QMutexLocker mutexLocker(&m_mutex);
        {
            QMutexLocker mutexLocker(&m_mutex);
            updateInner(other);
        }
        setStatus(other.m_status);
    }

    foreach (QnResourceConsumer *consumer, m_consumers)
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
        setStatus(parameters[QLatin1String("status")] == "A" ? QnResource::Online : QnResource::Offline, true);
}

QnId QnResource::getParentId() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(QnId parent)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_parentId = parent;
}


QString QnResource::getName() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_name;
}

void QnResource::setName(const QString& name)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_name = name;
}

unsigned long QnResource::flags() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_flags;
}

void QnResource::setFlags(unsigned long flags)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_flags = flags;
}

void QnResource::addFlag(unsigned long flag)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_flags |= flag;
}

void QnResource::removeFlag(unsigned long flag)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_flags &= ~flag;
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
    QnId resTypeId;
    {
        QMutexLocker mutexLocker(&m_mutex);
        if (!m_resourceParamList.isEmpty())
            return m_resourceParamList;
        resTypeId = m_typeId;
    }

    QnParamList resourceParamList;

    // 1. read Q_PROPERTY params
    const QMetaObject *mObject = metaObject();
    for (int i = 1; i < mObject->propertyCount(); ++i) { // from 1 to skip `objectName`
        const QMetaProperty mProperty = mObject->property(i);
        QnParamTypePtr paramType(new QnParamType);
        paramType->name = QString::fromLatin1(mProperty.name());
        paramType->ui = mProperty.isDesignable();
        paramType->isReadOnly = mProperty.isWritable();
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
            QMetaClassInfo mClassInfo = mObject->classInfo(j);
            if (qstrcmp(mClassInfo.name(), mProperty.name()) == 0)
                paramType->description = QCoreApplication::translate("QnResource", mClassInfo.value());
        }

        QnParam newParam(paramType, mProperty.read(this));
        resourceParamList.append(newParam);
    }

    // 2. read AppServer params
    if (QnResourceTypePtr resType = qnResTypePool->getResourceType(resTypeId)) {
        foreach (const QnParamTypePtr &paramType, resType->paramTypeList()) {
            QnParam newParam(paramType, paramType->default_value);
            resourceParamList.append(newParam);
        }
    }

    QMutexLocker mutexLocker(&m_mutex);
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
    const QnParamList &params = getResourceParamList();
    if (!params.contains(name))
    {
        cl_log.log("QnResource::getParam(): requested param does not exist!", cl_logWARNING);
        return false;
    }

    const QnParam &param = params.value(name);
    if (domain == QnDomainMemory)
    {
        QMutexLocker mutexLocker(&m_mutex);
        val = param.value();
        if (!param.isPhysical())
            val = property(param.name().toUtf8()).toString();
        return true;
    }
    else if (domain == QnDomainPhysical)
    {
        if (param.isPhysical() && getParamPhysical(param.name(), val)) {
            emit onParameterChanged(param.name(), val);
            return true;
        }
    }
    else if (domain == QnDomainDatabase)
    {
        if (param.isPhysical())
            return true;
    }

    return false;
}

bool QnResource::setParam(const QString& name, const QVariant& val, QnDomain domain)
{
    QnParamList &params = getResourceParamList();
    if (!params.contains(name))
    {
        cl_log.log("QnResource::setParam(): requested param does not exist!", cl_logWARNING);
        return false;
    }

    QnParam &param = params.value(name);

    if (param.isReadOnly())
    {
        cl_log.log("setParam: cannot set readonly param!", cl_logWARNING);
        return false;
    }

    if (domain == QnDomainPhysical)
    {
        if (!param.isPhysical() || !setParamPhysical(param.name(), val))
            return false;
    }

    //QnDomainMemory should changed anyway
    {
        QMutexLocker mutexLocker(&m_mutex);
        if (!param.setValue(val))
        {
            cl_log.log("cannot set such param!", cl_logWARNING);
            return false;

        }

        if (!param.isPhysical())
            setProperty(param.name().toUtf8(), val);
    }

    Q_EMIT onParameterChanged(param.name(), val);

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

void QnResource::getParamAsync(const QString &name, QnDomain domain)
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

void QnResource::setParamAsync(const QString& name, const QVariant& val, QnDomain domain)
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

void QnResource::setStatus(QnResource::Status newStatus, bool ignoreHandlers)
{
    Status oldStatus;
    {
        QMutexLocker mutexLocker(&m_mutex);
        oldStatus = m_status;
        m_status = newStatus;
    }

    if (oldStatus != newStatus && !ignoreHandlers)
        Q_EMIT statusChanged(oldStatus, newStatus);
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
    m_id = id;
}

QString QnResource::getUrl() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_url;
}

void QnResource::setUrl(const QString& value)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_url = value;
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

QStringList QnResource::tagList() const
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

int QnResource::commandProcQueueSize()
{
    return commandProcessor()->queueSize();
}

bool QnResource::commandProcHasSuchResourceInQueue(QnResourcePtr res)
{
    return commandProcessor()->hasSuchResourceInQueue(res);
}
