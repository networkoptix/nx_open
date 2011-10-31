#include "resource_command_consumer.h"
#include "resource_consumer.h"
#include "file_resource.h"
#include "core/dataprovider/abstract_streamdataprovider.h"
#include "core/resourcemanagment/resource_pool.h"

// Temporary until real ResourceFactory is implemented


QList<QnResourceFactoryPtr> QnResourceFactoryPool::m_factories;

CLDeviceCommandProcessor QnResource::m_commanproc;

QnResourceType::QnResourceType()
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
m_mutex(QMutex::Recursive)
{
}

QnResource::~QnResource()
{
    QMutexLocker locker(&m_mutex);
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

void QnResource::setParentId(const QnId& parent)
{
    QMutexLocker locker(&m_mutex);
	m_parentId = parent;
}

QnId QnResource::getParentId() const
{
    QMutexLocker locker(&m_mutex);
	return m_parentId;
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

/*
QnParamList& QnResource::getDeviceParamList()
{
    QMutexLocker locker(&m_mutex);
	if (m_resourceParamList.empty())
		m_resourceParamList = static_device_list[m_name];

	return m_resourceParamList;
}

const QnParamList& QnResource::getDeviceParamList() const
{
    QMutexLocker locker(&m_mutex);
	if (m_resourceParamList.empty())
		m_resourceParamList = static_device_list[m_name];

	return m_resourceParamList;

}
*/

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
    QMutexLocker locker(&m_mutex);
	QString result;
	QTextStream(&result) << getName() << "  " <<  getUniqueId();
	return result;
}

QString QnResource::toSearchString() const
{
    return toString();
}


bool QnResource::hasSuchParam(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return getResourceParamList().exists(name);
}

bool QnResource::getParamPhysical(const QString& name, QnValue& val)
{
    return false;
}

bool QnResource::setParamPhysical(const QString& name, const QnValue& val)
{
    return false;
}

bool QnResource::setSpecialParam(const QString& name, const QnValue& val, QnDomain domain)
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

    QMutexLocker locker(&m_mutex);



    if (domain == QnDomainMemory)
    {
        QnParam& param = getResourceParamList().get(name);
        val = param.value();
        return true;
    }
    else if (domain == QnDomainPhysical)
    {
        if (!getParamPhysical(name, val))
            return false;

        emit onParameterChanged(name, QString(val));
        return true;
    }
    else if (domain == QnDomainDatabase)
    {

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


    QMutexLocker locker(&m_mutex);
    QnParam& param = getResourceParamList().get(name);

    if (param.isReadOnly())
    {
        cl_log.log("setParam: cannot set readonly param!", cl_logWARNING);
        return false;
    }


    if (domain == QnDomainPhysical)
    {
        if (!setParamPhysical(name, val))
            return false;
    }

    // QnDomainMemory
    if (!param.setValue(val))
    {
        cl_log.log("cannot set such param!", cl_logWARNING);
        return false;
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
          //if (m_resource->hasSuchConsumer() isConnectedToTheResource())
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


QnParamList& QnResource::getResourceParamList() const
{
    QMutexLocker locker(&m_mutex);
    if (m_resourceParamList.empty()) 
    {
        QnResourceTypePtr resType = qnResTypePool->getResourceType(m_typeId);
        if (resType) {
            const QList<QnParamTypePtr>& paramTypes = resType->paramTypeList();
            foreach(QnParamTypePtr paramType, paramTypes)
            {
                QnParam newParam(paramType);
                newParam.setValue(paramType->default_value);
                cl_log.log(newParam.toDebugString(), cl_logALWAYS); // debug
                m_resourceParamList.put(newParam);
            }
        }
    }
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


bool QnResource::available() const
{
    return m_avalable;
}

void QnResource::setAvailable(bool av)
{
    m_avalable = av;
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

QnResourcePtr QnResourceFactory::createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters)
{
    return createResource(qnResTypePool->getResourceType(resourceTypeId), parameters);
}

QnResourcePtr QnResourceFactory::createResource(QnResourceTypePtr resourceType, const QnResourceParameters& parameters)
{
    qDebug() << "Creating resource with typeId" << resourceType->getName();

    for (QnResourceParameters::const_iterator ci = parameters.begin(); ci != parameters.end(); ci++)
    {
        qDebug() << ci.key() << " = " << ci.value();
    }

    // Stub. Should be implemented by concrete plugin.
    QnResourcePtr resource(new QnLocalFileResource(""));
    resource->deserialize(parameters);

    resource->setTypeId(resourceType->getId());

    return resource;
}

QnResourcePtr QnResourceFactoryPool::createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters)
{
    return createResource(qnResTypePool->getResourceType(resourceTypeId), parameters);
}

QnResourcePtr QnResourceFactoryPool::createResource(QnResourceTypePtr resourceType, const QnResourceParameters& parameters)
{
    if (m_factories.isEmpty())
        m_factories.append(QnResourceFactoryPtr(new QnResourceFactory()));

    QnResourcePtr result;

    foreach(QnResourceFactoryPtr factory, m_factories)
    {
        result = factory->createResource(resourceType, parameters);

        if (!result.isNull())
            return result;
    }

    return result;
}
