#include "resource.h"
#include "resourcecontrol/resource_command_consumer.h"
#include "resource_param.h"

static int inst = 0;

QnResourceCommandProcessor& commendProcessor()
{ 
    static QnResourceCommandProcessor inst;
    return inst;
}



QnResource::QnParamLists QnResource::static_resourcesParamLists; // list of all supported devices params list


QnResource::QnResource():
m_typeFlags(0),
m_avalable(true),
m_mutex(QMutex::Recursive)
{
	++inst;	

    m_parentId.setEmpty();
};

QnResource::~QnResource()
{
	--inst;	
	cl_log.log("inst", inst, cl_logDEBUG1);
};

void QnResource::addFlag(unsigned long flag)
{
    QMutexLocker locker(&m_mutex);
    m_typeFlags |= flag;
}

void QnResource::removeFlag(unsigned long flag)
{
    QMutexLocker locker(&m_mutex);
    m_typeFlags &= (~flag);
}

bool QnResource::checkFlag(unsigned long flag) const
{
    QMutexLocker locker(&m_mutex);
    return m_typeFlags & flag;
}


QnId QnResource::getId() const
{
    QMutexLocker locker(&m_mutex);
    return m_Id;
}

void QnResource::setId(const QnId& id)
{
    QMutexLocker locker(&m_mutex);
    m_Id = id;
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

QDateTime QnResource::getLastDiscoveredTime() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastDiscoveredTime;
}

void QnResource::setLastDiscoveredTime(QDateTime time)
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

void QnResource::addTag(const QString& tag)
{
    QMutexLocker locker(&m_mutex);
    if (!m_tags.contains(tag))
        m_tags.push_back(tag);
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

QString QnResource::toString() const
{
    QString result;
    QTextStream(&result) << getName();
    return result;
}

QString QnResource::toSearchString() const
{
    QString result;
    QTextStream t (&result);

    t << toString() << " ";

    QMutexLocker locker(&m_mutex);

    foreach(QString tag, m_tags)
        t << tag << " ";

    return result;
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

    if (setSpecialParam(name, val, domain)) // try special first
        return true;

    QMutexLocker locker(&m_mutex);

    if (domain == QnDomainMemory) 
    {
        QnParam& param = getResourceParamList().get(name);
        val = param.value;
        return true;
    }
    else if (domain == QnDomainPhysical)
    {
        if (getParamPhysical(name, val))
        {
            emit onParametrChanged(name, QString(val));
            return true;
        }
        else
        {
            return false;
        }
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

    if (param.readonly)
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

    emit onParametrChanged(name, QString(val));

    return true;

}

void QnResource::setParamAsynch(const QString& name, const QnValue& val, QnDomain domain)
{

    class QnResourceSetParamCommand : public QnResourceCommand
    {
    public:
        QnResourceSetParamCommand(QnResourcePtr res, const QString& name, const QnValue& val, QnDomain domain):
          QnResourceCommand(res),
              m_name(name),
              m_val(val),
              m_domain(domain)
          {

          }

          virtual void beforeDisconnectFromResource(){};

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

    QnResourceSetParamCommandPtr command ( new QnResourceSetParamCommand(QnResourcePtr(this), name, val, domain) );
    addCommandToProc(command);

}

bool QnResource::unknownResource() const
{
    return getName().isEmpty();
}


QnParamList& QnResource::getResourceParamList()
{
    QMutexLocker locker(&m_mutex);

	if (m_deviceParamList.empty())
		m_deviceParamList = static_resourcesParamLists[m_name];

	return m_deviceParamList;
}

const QnParamList& QnResource::getResourceParamList() const
{
    QMutexLocker locker(&m_mutex);

	if (m_deviceParamList.empty())
		m_deviceParamList = static_resourcesParamLists[m_name];

	return m_deviceParamList;

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
//========================================================================================
// static 

void QnResource::startCommandProc() 
{
    commendProcessor().start();
};

void QnResource::stopCommandProc() 
{
    commendProcessor().stop();
};

void QnResource::addCommandToProc(QnAbstractDataPacketPtr data) 
{
    commendProcessor().putData(data);
};

int QnResource::commandProcQueSize() 
{
    return commendProcessor().queueSize();
}

bool QnResource::commandProcHasSuchResourceInQueue(QnResourcePtr res) 
{
    return commendProcessor().hasSuchResourceInQueue(res);
}

//========================================================================================

struct ResourcesBasicInfoHelper
{
    ResourcesBasicInfoHelper(QnResourcePtr d)
    {
        resource = d;
    }

    void f()
    {
        resource->getBasicInfo();
    }

    QnResourcePtr resource;
};


void getResourcesBasicInfo(QnResourceList& lst, int threads)
{
	// cannot make concurrent work with pointer CLDevice* ; => so extra steps needed
    cl_log.log("Getting resource info...", cl_logDEBUG1);
	QTime time;
	time.start();

	QList<ResourcesBasicInfoHelper> local_list;

	QThreadPool* global = QThreadPool::globalInstance();
	for (int i = 0; i < threads; ++i ) global->releaseThread();
	QtConcurrent::blockingMap(local_list, &ResourcesBasicInfoHelper::f);
	for (int i = 0; i < threads; ++i )global->reserveThread();

	CL_LOG(cl_logDEBUG1)
	{
		cl_log.log("Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);

		foreach(QnResourcePtr resource, lst)
        {
			cl_log.log(resource->toString(), cl_logDEBUG1);
        }

	}

}


//==================================================================================================================

bool hasEqualResource(const QnResourceList& lst, const QnResourcePtr res)
{
    foreach(QnResourcePtr resource, lst)
    {
        if (res->equalsTo(resource))
            return true;
    }

    return false;
}