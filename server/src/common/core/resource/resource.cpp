#include "resource.h"
#include "video_resource_layout.h"

static int inst = 0;

QnResourceCommandProcessor QnResource::static_commandProc;

QnResource::QnParamLists QnResource::static_resourcesParamLists; // list of all supported devices params list
QnResource::QnParamLists QnResource::static_stream_list; // list of all supported streams params list

QnResource::QnResource():
m_deviceTypeFlags(0),
m_videolayout(0)
{
	inst++;	
};

QnResource::~QnResource()
{
	delete m_videolayout;
	inst--;	
	cl_log.log("inst", inst, cl_logDEBUG1);
};

void QnResource::setParentId(QnId parentid)
{
	m_parentId = parentid;
}

QnId QnResource::getParentId() const
{
	return m_parentId;
}

QnId QnResource::getId() const
{
	return m_Id;
}

void QnResource::setId(const QnId& id)
{
	m_Id = id;
}

QString QnResource::getName() const
{
	return m_name;
}

void QnResource::setName(const QString& name)
{
	m_name = name;
}

QnResourceStatus& QnResource::getStatus()
{
	return m_status;
}

void  QnResource::setStatus(const QnResourceStatus& status)
{
	m_status = status;
}

QnParamList& QnResource::getResourceParamList()
{
	if (m_deviceParamList.empty())
		m_deviceParamList = static_resourcesParamLists[m_name];

	return m_deviceParamList;
}

const QnParamList& QnResource::getResourceParamList() const
{
	if (m_deviceParamList.empty())
		m_deviceParamList = static_resourcesParamLists[m_name];

	return m_deviceParamList;

}

bool QnResource::checkDeviceTypeFlag(unsigned long flag) const
{
	return m_deviceTypeFlags & flag;
}

void QnResource::addDeviceTypeFlag(unsigned long flag)
{
	m_deviceTypeFlags |= flag;
}

void QnResource::removeDeviceTypeFlag(unsigned long flag)
{
	m_deviceTypeFlags &= ~flag;
}

QnParamList& QnResource::getStreamParamList()
{
	if (m_streamParamList.empty())
		m_streamParamList = static_stream_list[m_name];

	return m_streamParamList;
}

const QnParamList& QnResource::getStreamParamList() const
{
	if (m_streamParamList.empty())
		m_streamParamList = static_stream_list[m_name];

	return m_streamParamList;
}


void QnResource::setVideoLayout(QnVideoResoutceLayout* layout)
{
	delete m_videolayout;
	m_videolayout = layout;
}

const QnVideoResoutceLayout* QnResource::getVideoLayout() const
{
	if (!m_videolayout)
		m_videolayout = new CLDefaultDeviceVideoLayout();

	return m_videolayout;

}

QString QnResource::toString() const
{
	QString result;
	QTextStream(&result) << getName() << "  " <<  getId().toString();
	return result;
}



#ifndef _WIN32
struct T
{
    T(QnResourcePtr d)
    {
        resource = d;
    }

    void f()
    {
        resource->getBasicInfo();
    }

    QnResourcePtr resource;
};
#endif

void QnResource::getDevicesBasicInfo(QnResourceList& lst, int threads)
{
	// cannot make concurrent work with pointer CLDevice* ; => so extra steps needed

#ifdef _WIN32
    struct T
    {
        T(QnResourcePtr d)
        {
            resource = d;
        }

        void f()
        {
            resource->getBasicInfo();
        }

        QnResourcePtr resource;
    };
#endif

    cl_log.log("Getting resource info...", cl_logDEBUG1);
	QTime time;
	time.start();

	QList<T> local_list;


    foreach(QnResourcePtr resource, lst)
    {
        if (resource->getStatus().checkFlag(QnResourceStatus::CONFLICTING)==false)
            local_list.push_back(T(resource));
    }

	QThreadPool* global = QThreadPool::globalInstance();
	for (int i = 0; i < threads; ++i ) global->releaseThread();
	QtConcurrent::blockingMap(local_list, &T::f);
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

bool QnResource::getParam(const QString& name, QnValue& val, bool resynch) 
{
	if (!getResourceParamList().exists(name))
	{
		cl_log.log("getParam: requested param does not exist!", cl_logWARNING);
		return false;
	}

	return true;
}

bool QnResource::setParam(const QString& name, const QnValue& val)
{
	if (!getResourceParamList().exists(name))
	{
		cl_log.log("setParam: requested param does not exist!", cl_logWARNING);
		return false;
	}

	if (getResourceParamList().get(name).value.readonly)
	{
		cl_log.log("setParam: cannot set readonly param!", cl_logWARNING);
		return false;
	}

	return true;

}

bool QnResource::setParamAsynch(const QString& name, const QnValue& val)
{

	class QnResourceSetParamCommand : public QnResourceCommand
	{
	public:
		QnResourceSetParamCommand(QnResource* dev, const QString& name, const QnValue& val):
		QnResourceCommand(dev),
		m_name(name),
		m_val(val)
		{

		}

		void execute()
		{
			m_resource->setParam(m_name, m_val);
		}
	private:
		QString m_name;
		QnValue m_val;

	};

    typedef QSharedPointer<QnResourceSetParamCommand> QnResourceSetParamCommandPtr;

	QnResourceSetParamCommandPtr command ( new QnResourceSetParamCommand(this, name, val) );
	static_commandProc.putData(command);

	return true;
}

//==================================================================================================================

bool hasEqual(const QnResourceList& lst, const QnResourcePtr res)
{
    foreach(QnResourcePtr resource, lst)
    {
        if (res->equalsTo(resource))
            return true;
    }

    return false;
}