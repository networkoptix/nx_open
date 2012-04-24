#include "video_server.h"

#include <QtCore/QUrl>
#include "utils/common/delete_later.h"
#include "api/SessionManager.h"

QnLocalVideoServerResource::QnLocalVideoServerResource()
    : QnResource()
{
    //setTypeId(qnResTypePool->getResourceTypeId("", QLatin1String("LocalServer"))); // ###
    addFlags(server | local);
    setName(QLatin1String("Local"));
    setStatus(Online);
}

QString QnLocalVideoServerResource::getUniqueId() const
{
    return QLatin1String("LocalServer");
}


QnVideoServerResource::QnVideoServerResource():
    QnResource()
    //,m_rtspListener(0)
{
    setTypeId(qnResTypePool->getResourceTypeId("", QLatin1String("Server")));
    addFlags(server | remote);
    m_primaryIFSelected = false;
}

QnVideoServerResource::~QnVideoServerResource()
{
    //delete m_rtspListener;
}

QString QnVideoServerResource::getUniqueId() const
{
    QMutexLocker mutexLocker(&m_mutex); // needed here !!!
    QnVideoServerResource* nonConstThis = const_cast<QnVideoServerResource*> (this);
    if (!getId().isValid())
        nonConstThis->setId(QnId::generateSpecialId());
    return QLatin1String("Server ") + getId().toString();
}

void QnVideoServerResource::setApiUrl(const QString& restUrl)
{
    QMutexLocker lock(&m_mutex);
    if (restUrl != m_apiUrl)
    {
        m_apiUrl = restUrl;

        /* We want the video server connection to be deleted in its associated thread, 
         * no matter where the reference count reached zero. Hence the custom deleter. */
        m_restConnection = QnVideoServerConnectionPtr(new QnVideoServerConnection(restUrl), &qnDeleteLater);
    }
}

QString QnVideoServerResource::getApiUrl() const
{
    QMutexLocker lock(&m_mutex);
    return m_apiUrl;
}

void QnVideoServerResource::setNetAddrList(const QList<QHostAddress>& netAddrList)
{
    QMutexLocker lock(&m_mutex);
    m_netAddrList = netAddrList;
}

QList<QHostAddress> QnVideoServerResource::getNetAddrList()
{
    QMutexLocker lock(&m_mutex);
    return m_netAddrList;
}

QnVideoServerConnectionPtr QnVideoServerResource::apiConnection()
{
    return m_restConnection;
}

QnResourcePtr QnVideoServerResourceFactory::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    Q_UNUSED(resourceTypeId)

    QnResourcePtr result(new QnVideoServerResource());
    result->deserialize(parameters);

    return result;
}

QnStorageResourceList QnVideoServerResource::getStorages() const
{
    return m_storages;
}

void QnVideoServerResource::setStorages(const QnStorageResourceList &storages)
{
    m_storages = storages;
}


// --------------------------------------------------

class TestConnectionTask: public QRunnable
{
public:
    TestConnectionTask(QnVideoServerResource* owner, const QUrl& url): m_owner(owner), m_url(url) {}
    void run()
    {
        QByteArray reply;
        QByteArray errorString;
        SessionManager::instance()->sendGetRequest(m_url.toString(), "RecordedTimePeriods", QnRequestParamList(), reply, errorString);
        if (reply.contains("Parameter startTime must be provided"))
        {
            // server OK
            m_owner->setPrimaryIF(m_url.host());
        }
    }
private:
    QUrl m_url;
    QnVideoServerResource* m_owner;
};

void QnVideoServerResource::setPrimaryIF(const QString& primaryIF)
{
    QMutexLocker lock(&m_mutex);
    if (m_primaryIFSelected)
        return;
    m_primaryIFSelected = true;
    
    QUrl apiUrl(getApiUrl());
    apiUrl.setHost(primaryIF);
    setApiUrl(apiUrl.toString());

    QUrl url(getUrl());
    url.setHost(primaryIF);
    setUrl(url.toString());
    setName(QString("Server ") + primaryIF);
}

void QnVideoServerResource::determineOptimalNetIF()
{
    QMutexLocker lock(&m_mutex);
    if (m_prevNetAddrList == m_netAddrList)
        return;
    m_prevNetAddrList = m_netAddrList;
    m_primaryIFSelected = false;

    for (int i = 0; i < m_netAddrList.size(); ++i)
    {
        QUrl url(m_apiUrl);
        url.setHost(m_netAddrList[i].toString());
        TestConnectionTask *task = new TestConnectionTask(this, url);
        QThreadPool::globalInstance()->start(task);
    }
}

void QnVideoServerResource::updateInner(QnResourcePtr other) 
{
    QMutexLocker lock(&m_mutex);

    QnResource::updateInner(other);

    QnVideoServerResourcePtr localOther = other.dynamicCast<QnVideoServerResource>();
    if(localOther) {
        m_netAddrList = localOther->m_netAddrList;
        setApiUrl(localOther->m_apiUrl);

        QnStorageResourceList otherStorages = localOther->getStorages();
        
        // keep index uhcnaged (app server does not provide such info
        foreach(QnStorageResourcePtr storage, m_storages)
        {
            for (int i = 0; i < otherStorages.size(); ++i)
            {
                if (otherStorages[i]->getId() == storage->getId()) {
                    otherStorages[i]->setIndex(storage->getIndex());
                    break;
                }
            }
        }

        setStorages(otherStorages);
    }
	determineOptimalNetIF();
}
