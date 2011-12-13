#include "resource_command_consumer.h"
#include "resource_consumer.h"
#include "file_resource.h"
#include "core/dataprovider/abstract_streamdataprovider.h"
#include "core/resourcemanagment/resource_pool.h"

// Temporary until real ResourceFactory is implemented


QnResourceCommandProcessor QnResource::m_commanproc;

QnResourceType::QnResourceType()
    : m_isCameraSet(false)
{
}

/*
QnResourceType::QnResourceType(const QString& name): m_name(name)
{
}
*/

QnResourceType::~QnResourceType()
{

}

QnResource::QnResource():
m_flags(0),
m_avalable(true),
m_mutex(QMutex::Recursive),
m_status(Offline)
{
}

QnResource::~QnResource()
{
    disconnectAllConsumers();
}

void QnResource::deserialize(const QnResourceParameters& parameters)
{
    const char* ID = "id";
    const char* PARENT_ID = "parentId";
    const char* TYPE_ID = "typeId";
    const char* NAME = "name";
    const char* URL = "url";

    if (parameters.contains(ID))
        setId(parameters[ID]);

    if (parameters.contains(TYPE_ID))
        setTypeId(parameters[TYPE_ID]);

    if (parameters.contains(PARENT_ID))
        setParentId(parameters[PARENT_ID]);

    if (parameters.contains(NAME))
        setName(parameters[NAME]);

    if (parameters.contains(URL))
        setUrl(parameters[URL]);
}

QnId QnResource::getParentId() const
{
    QMutexLocker locker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(const QnId& parent)
{
    QMutexLocker locker(&m_mutex);
    m_parentId = parent;
}


QString QnResource::getName() const
{
    QMutexLocker locker(&m_mutex);
    return m_name;
}

void QnResource::setName(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    m_name = name;
}

unsigned long QnResource::flags() const
{
    QMutexLocker locker(&m_mutex);
    return m_flags;
}

bool QnResource::checkFlag(unsigned long flag) const
{
    QMutexLocker locker(&m_mutex);
    return (m_flags & flag) == flag;
}

void QnResource::addFlag(unsigned long flag)
{
    QMutexLocker locker(&m_mutex);
    m_flags |= flag;
}

void QnResource::removeFlag(unsigned long flag)
{
    QMutexLocker locker(&m_mutex);
    m_flags &= ~flag;
}

bool QnResource::associatedWithFile() const
{
    QMutexLocker locker(&m_mutex);
    return checkFlag(QnResource::ARCHIVE) || checkFlag(QnResource::SINGLE_SHOT);
}

QString QnResource::toString() const
{
    return getName() + QLatin1String("  ") + getUniqueId();
}

QString QnResource::toSearchString() const
{
    return toString();
}


bool QnResource::hasSuchParam(const QString& name) const
{
    return getResourceParamList().exists(name);
}

bool QnResource::getParamPhysical(const QString& /*name*/, QnValue& /*val*/)
{
    return false;
}

bool QnResource::setParamPhysical(const QString& /*name*/, const QnValue& /*val*/)
{
    return false;
}

bool QnResource::setSpecialParam(const QString& /*name*/, const QnValue& /*val*/, QnDomain /*domain*/)
{
    return false;
}

bool QnResource::getParam(const QString& name, QnValue& val, QnDomain domain )
{

    if (!hasSuchParam(name))
    {
        cl_log.log("getParam: requested param does not exist!", cl_logWARNING);
        return false;
    }


    QnParam& param = getResourceParamList().get(name);
    if (domain == QnDomainMemory)
    {
        QMutexLocker locker(&m_mutex);
        val = param.value();
        if (param.isStatic())
            val = this->property(name.toUtf8()).toString();
        return true;
    }
    else if (domain == QnDomainPhysical)
    {
        if (param.isStatic())
            return false;

        if (!getParamPhysical(name, val))
            return false;

        emit onParameterChanged(name, QString(val));
        return true;
    }
    else if (domain == QnDomainDatabase)
    {
        if (param.isStatic())
            return false;
    }

    return true;
}

void QnResource::getParamAsynch(const QString& name, QnValue& val, QnDomain domain )
{

}

bool QnResource::setParam(const QString& name, const QnValue& val, QnDomain domain)
{
    if (!hasSuchParam(name))
    {
        cl_log.log("setParam: requested param does not exist!", cl_logWARNING);
        return false;
    }


    QnParam& param = getResourceParamList().get(name);

    if (param.isReadOnly())
    {
        cl_log.log("setParam: cannot set readonly param!", cl_logWARNING);
        return false;
    }


    if (domain == QnDomainPhysical)
    {
        if (param.isStatic())
            return false;

        if (!setParamPhysical(name, val))
            return false;
    }

    // QnDomainMemory
    {
        QMutexLocker locker(&m_mutex);
        if (!param.setValue(val))
        {
            cl_log.log("cannot set such param!", cl_logWARNING);
            return false;
        }
        if (param.isStatic())
            setProperty(param.name().toUtf8(), (QString) val);
    }
    emit onParameterChanged(name, QString(val));

    return true;
}

class QnResourceSetParamCommand : public QnResourceCommand
{
public:
    QnResourceSetParamCommand(QnResourcePtr res, const QString& name, const QnValue& val, QnDomain domain):
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
    QnValue m_val;
    QnDomain m_domain;
};

typedef QSharedPointer<QnResourceSetParamCommand> QnResourceSetParamCommandPtr;

void QnResource::setParamAsynch(const QString& name, const QnValue& val, QnDomain domain)
{
    QnResourceSetParamCommandPtr command ( new QnResourceSetParamCommand(toSharedPointer(), name, val, domain) );
    addCommandToProc(command);
}

bool QnResource::unknownResource() const
{
    return getName().isEmpty();
}

QnParamType::DataType variantTypeToPropType(QVariant::Type type)
{
    switch (type)
    {
        case QVariant::Bool:
            return QnParamType::Boolen;
        case QVariant::UInt:
        case QVariant::Int:
        case QVariant::ULongLong:
        case QVariant::LongLong:
            return QnParamType::MinMaxStep;
        default:
            return QnParamType::Value;
    }
}

QnParamList& QnResource::getResourceParamList() const
{
    QnId restypeid;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_resourceParamList.empty())
            return m_resourceParamList;
        restypeid = m_typeId;
    }

    QnResourceTypePtr resType = qnResTypePool->getResourceType(restypeid);
    if (!resType)
        return m_resourceParamList;

    QnParamList resourceParamList;

    // 1. read Q_PROPERTY params
    for (int i = 0; i < metaObject()->propertyCount(); ++i)
    {
        QMetaProperty prop =  metaObject()->property(i);
        QnParamTypePtr paramType(new QnParamType());
        paramType->type = variantTypeToPropType(prop.type());
        paramType->name = prop.name();
        paramType->ui = prop.isUser();
        paramType->isStatic = true;
        //paramType = resType->addParamType(paramType);
        QnParam newParam(paramType);
        resourceParamList.put(newParam);
    }

    // 2. read AppServer params

    const QList<QnParamTypePtr>& paramTypes = resType->paramTypeList();
    foreach(QnParamTypePtr paramType, paramTypes)
    {
        QnParam newParam(paramType);
        newParam.setValue(paramType->default_value);
        //cl_log.log(newParam.toDebugString(), cl_logALWAYS); // debug
        resourceParamList.put(newParam);
    }

    QMutexLocker locker(&m_mutex);
    if (m_resourceParamList.empty())
        m_resourceParamList = resourceParamList;

    return m_resourceParamList;
}

QnId QnResource::getTypeId() const
{
    QMutexLocker locker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(const QnId& id)
{
    QMutexLocker locker(&m_mutex);
    m_typeId = id;
}

QnResource::Status QnResource::getStatus() const
{
    QMutexLocker locker(&m_mutex);
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
        QMutexLocker locker(&m_mutex);
        old_status = m_status;
        m_status = status;
    }
    emit onStatusChanged(old_status, status);
}


QDateTime QnResource::getLastDiscoveredTime() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastDiscoveredTime;
}

void QnResource::setLastDiscoveredTime(const QDateTime &time)
{
    QMutexLocker locker(&m_mutex);
    m_lastDiscoveredTime = time;
}



QnId QnResource::getId() const
{
    QMutexLocker locker(&m_mutex);
    return m_id;
}
void QnResource::setId(const QnId& id) {
    QMutexLocker locker(&m_mutex);
    m_id = id;
}

QString QnResource::getUrl() const
{
    QMutexLocker locker(&m_mutex);
    return m_url;
}

void QnResource::setUrl(const QString& value)
{
    QMutexLocker locker(&m_mutex);
    m_url = value;
}

void QnResource::addTag(const QString& tag)
{
    QMutexLocker locker(&m_mutex);
    if (!m_tags.contains(tag))
        m_tags.push_back(tag);
}

void QnResource::setTags(const QStringList& tags)
{
    QMutexLocker locker(&m_mutex);
    m_tags = tags;
}

void QnResource::removeTag(const QString& tag)
{
    QMutexLocker locker(&m_mutex);
    m_tags.removeAll(tag);
}

bool QnResource::hasTag(const QString& tag) const
{
    QMutexLocker locker(&m_mutex);
    return m_tags.contains(tag);
}

QStringList QnResource::tagList() const
{
    QMutexLocker locker(&m_mutex);
    return m_tags;
}

QnResourcePtr QnResource::toSharedPointer() const
{
    QnResourcePtr res = qnResPool->getResourceById(getId());
    Q_ASSERT_X(res != 0, Q_FUNC_INFO, "Resource not found");
    return res;
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
