#include "resource.h"

#include <QtCore/QtConcurrentMap>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QThreadPool>

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

#include "resourcecontrol/resource_command_consumer.h"
#include "resource_param.h"

static int inst = 0;

QnResourceCommandProcessor& commendProcessor()
{
    static QnResourceCommandProcessor inst;
    return inst;
}



QnResource::QnParamLists QnResource::staticResourcesParamLists; // list of all supported devices params list


QnResource::QnResource()
	: m_mutex(QMutex::Recursive),
	m_typeFlags(0),
	m_avalable(true)
{
	++inst;

	m_parentId.setEmpty();
}

QnResource::~QnResource()
{
	--inst;
	cl_log.log("inst", inst, cl_logDEBUG1);
}

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

QnResourceTypeId QnResource::getTypeId() const
{
    QMutexLocker locker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(const QnResourceTypeId& id)
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

    QMutexLocker locker(&m_mutex);

    if (setSpecialParam(name, val, domain)) // try special first
        return true;


    if (domain == QnDomainMemory)
    {
        QnParam& param = getResourceParamList().get(name);
        val = param.value;
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

	if (m_resourceParamList.empty())
		m_resourceParamList = staticResourcesParamLists[getTypeId()];

	return m_resourceParamList;
}

const QnParamList& QnResource::getResourceParamList() const
{
	QMutexLocker locker(&m_mutex);

	if (m_resourceParamList.empty())
		m_resourceParamList = staticResourcesParamLists[getTypeId()];

	return m_resourceParamList;

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
}

void QnResource::stopCommandProc()
{
    commendProcessor().stop();
}

void QnResource::addCommandToProc(QnAbstractDataPacketPtr data)
{
    commendProcessor().putData(data);
}

int QnResource::commandProcQueSize()
{
    return commendProcessor().queueSize();
}

bool QnResource::commandProcHasSuchResourceInQueue(QnResourcePtr res)
{
    return commendProcessor().hasSuchResourceInQueue(res);
}

bool QnResource::loadDevicesParam(const QString& fileName)
{
    QString error;
    QFile file(fileName);

    if (!file.exists())
    {
        error = "loadDevicesParam: Cannot open file ";
        error += fileName;
        cl_log.log(error, cl_logERROR);
        return false;
    }

    QString errorStr;
    int errorLine;
    int errorColumn;
    QDomDocument doc;

    if (!doc.setContent(&file, &errorStr, &errorLine,&errorColumn))
    {
        QTextStream str(&error);
        str << "File " << fileName << "; Parse error at line " << errorLine << " column " << errorColumn << " : " << errorStr;
        cl_log.log(error, cl_logERROR);
        return false;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != "devices")
        return false;

    QDomNode node = root.firstChild();

    while (!node.isNull())
    {
        if (node.toElement().tagName() == "resource")
        {
            if (!parseResource(node.toElement()))
                return false;
        }

        node = node.nextSibling();
    }

    return true;
}

bool QnResource::parseResource(const QDomElement &resource)
{
    QString error;
    if (!resource.hasAttribute("id"))
    {
        error = "Cannot find id of the resource.";
        cl_log.log(error, cl_logERROR);
        return false;
    }

    QString resourceName = resource.attribute("id");

    QnParamLists::iterator it1 = staticResourcesParamLists.find(resourceName);


    if (it1!=staticResourcesParamLists.end())
    {
        QTextStream str(&error);
        str << "Such resource " << resourceName << " already exists.";
        cl_log.log(error, cl_logERROR);
        return false;
    }

    QnParamList device_param_list;

    if (resource.hasAttribute("public"))
    {
        QString values = resource.attribute("public");
        QStringList lst = values.split(",");
        QStringListIterator lst_iter(lst);
        while(lst_iter.hasNext())
        {
            QString val = lst_iter.next().trimmed();
            QnParamLists::iterator it1 = staticResourcesParamLists.find(val);

            if (it1!=staticResourcesParamLists.end())
                device_param_list.inheritedFrom(it1.value());
            else
                cl_log.log("Smth wrong with public tag for ", resourceName, cl_logWARNING);

        }

    }

    //global_params
    QDomNode node = resource.firstChild();
    if (node.isNull())
    {
        staticResourcesParamLists[resourceName] = device_param_list;
        return true; // just public make sence
    }

    QDomElement element = node.toElement();

    if (element.tagName()!="global_params" && element.tagName()!="stream_params")
    {
        QTextStream str(&error);
        str << "Cannot find global_params and stream_params in " << resourceName ;
        return false;
    }

    //===============================================================
    if (element.tagName()=="global_params" )
    {
        QDomNode paramNode = element.firstChild();
        while (!paramNode .isNull())
        {
            if (paramNode.toElement().tagName() == "param")
            {
                if (!parseParam(paramNode.toElement(), device_param_list))
                    return false;
            }
            paramNode  = paramNode.nextSibling();
        }

        staticResourcesParamLists[resourceName] = device_param_list;

        // move to stream_params
        node = node.nextSibling();
        if (node.isNull())
            return true;
        element = node.toElement();

    }

    return true;
}

bool QnResource::parseParam(const QDomElement &element, QnParamList& paramlist)
{
    QString error;

    if (!element.hasAttribute("name"))
    {
        error = "Cannot find name attribute.";
        cl_log.log(error, cl_logERROR);
        return false;
    }

    QString name = element.attribute("name");

    QnParam param = paramlist.exists(name) ? paramlist.get(name) : QnParam();
    param.name = name;

    if (param.type == QnParam::None) // param type is not defined yet
    {
        if (!element.hasAttribute("type"))
        {
            error = "Cannot find type attribute.";
            return false;
        }

        QString type = element.attribute("type").toLower();

        if (type=="param")
            param.type = QnParam::Value;
        else if (type=="novalue")
            param.type = QnParam::None;
        else if (type=="minmaxstep")
            param.type = QnParam::MinMaxStep;
        else if (type=="boolen")
            param.type = QnParam::Boolen;
        else if (type=="onoff")
            param.type = QnParam::OnOff;
        else if (type=="enumeration")
            param.type = QnParam::Enumeration;
        else if (type=="button")
            param.type = QnParam::Button;
        else
        {
            error = "Unsupported param type fund";
            return false;
        }
    }

    if (element.hasAttribute("group"))
        param.group = element.attribute("group");

    if (element.hasAttribute("sub_group"))
        param.subgroup = element.attribute("sub_group");

    if (element.hasAttribute("min"))
        param.min_val = element.attribute("min");

    if (element.hasAttribute("description"))
        param.description = element.attribute("description");

    if (element.hasAttribute("max"))
        param.max_val = element.attribute("max");

    if (element.hasAttribute("step"))
        param.step = element.attribute("step");

    if (element.hasAttribute("default_value"))
        param.default_value = element.attribute("default_value");

    if (element.hasAttribute("paramNetHelper"))
        param.paramNetHelper = element.attribute("paramNetHelper");

    if (element.hasAttribute("ui"))
        param.ui = element.attribute("ui").toInt();

    if (element.hasAttribute("readonly"))
    {
        if (element.attribute("readonly")=="true")
            param.readonly = true;
        else
            param.readonly = false;
    }

    if (element.hasAttribute("values"))
    {
        QString values = element.attribute("values");
        QStringList lst = values.split(",");
        QStringListIterator lst_iter(lst);
        while(lst_iter.hasNext())
        {
            QString val = lst_iter.next().trimmed();
            param.possible_values.push_back(val);
        }

    }

    if (element.hasAttribute("ui_values"))
    {
        QString values = element.attribute("ui_values");
        QStringList lst = values.split(",");
        QStringListIterator lst_iter(lst);
        while(lst_iter.hasNext())
        {
            QString val = lst_iter.next().trimmed();
            param.ui_possible_values.push_back(val);
        }

    }

    if (element.hasAttribute("param"))
        param.value = element.attribute("param");
    else
        param.value = param.default_value;

    paramlist.put(param);

    return true;
}


//========================================================================================

struct ResourcesBeforeUseHelper
{
    ResourcesBeforeUseHelper(QnResourcePtr d)
    {
        resource = d;
    }

    void f()
    {
        resource->beforeUse();
    }

    QnResourcePtr resource;
};


void resourcesBeforeUseHelper(QnResourceList& lst, int threads)
{
    // cannot make concurrent work with pointer CLDevice* ; => so extra steps needed
    cl_log.log("Calling BeforeUse for resources...", cl_logDEBUG1);
    QTime time;
    time.start();

    QList<ResourcesBeforeUseHelper> local_list;

	QThreadPool* global = QThreadPool::globalInstance();
	for (int i = 0; i < threads; ++i ) global->releaseThread();
	QtConcurrent::blockingMap(local_list, &ResourcesBeforeUseHelper::f);
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
