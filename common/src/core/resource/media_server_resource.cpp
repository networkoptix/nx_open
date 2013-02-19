#include "media_server_resource.h"

#include <QtCore/QUrl>
#include "utils/common/delete_later.h"
#include "api/session_manager.h"
#include "utils/common/sleep.h"

class QnMediaServerResourceGuard: public QObject {
public:
    QnMediaServerResourceGuard(const QnMediaServerResourcePtr &resource): m_resource(resource) {}

private:
    QnMediaServerResourcePtr m_resource;
};


QnLocalMediaServerResource::QnLocalMediaServerResource(): 
    QnResource()
{
    //setTypeId(qnResTypePool->getResourceTypeId("", QLatin1String("LocalServer"))); // ###
    addFlags(QnResource::server | QnResource::local);
    removeFlags(QnResource::media); // TODO: is this call needed here?

    setName(QLatin1String("Local"));
    setStatus(Online);
}

QString QnLocalMediaServerResource::getUniqueId() const
{
    return QLatin1String("LocalServer");
}


QnMediaServerResource::QnMediaServerResource():
    QnResource(),
    m_panicMode(PM_None),
    m_guard(NULL)
{
    setTypeId(qnResTypePool->getResourceTypeId(QString(), QLatin1String("Server")));
    addFlags(QnResource::server | QnResource::remote);
    removeFlags(QnResource::media); // TODO: is this call needed here?
    setName(tr("Server"));

    m_primaryIFSelected = false;
}

QnMediaServerResource::~QnMediaServerResource()
{
    QMutexLocker lock(&m_mutex); // TODO: #Elric remove once m_guard hell is removed.
}

QString QnMediaServerResource::getUniqueId() const
{
    QMutexLocker mutexLocker(&m_mutex); // needed here !!!
    QnMediaServerResource* nonConstThis = const_cast<QnMediaServerResource*> (this);
    if (!getId().isValid())
        nonConstThis->setId(QnId::generateSpecialId());
    return QLatin1String("Server ") + getId().toString();
}

void QnMediaServerResource::setApiUrl(const QString& restUrl)
{
    QMutexLocker lock(&m_mutex);
    if (restUrl != m_apiUrl)
    {
        m_apiUrl = restUrl;

        /* We want the video server connection to be deleted in its associated thread, 
         * no matter where the reference count reached zero. Hence the custom deleter. */
        m_restConnection = QnMediaServerConnectionPtr(new QnMediaServerConnection(toSharedPointer()), &qnDeleteLater);
    }
}

QString QnMediaServerResource::getApiUrl() const
{
    QMutexLocker lock(&m_mutex);
    return m_apiUrl;
}

void QnMediaServerResource::setStreamingUrl(const QString& value)
{
    QMutexLocker lock(&m_mutex);
    m_streamingUrl = value;
}

QString QnMediaServerResource::getStreamingUrl() const
{
    QMutexLocker lock(&m_mutex);
    return m_streamingUrl;
}

void QnMediaServerResource::setNetAddrList(const QList<QHostAddress>& netAddrList)
{
    QMutexLocker lock(&m_mutex);
    m_netAddrList = netAddrList;
}

QList<QHostAddress> QnMediaServerResource::getNetAddrList()
{
    QMutexLocker lock(&m_mutex);
    return m_netAddrList;
}

QnMediaServerConnectionPtr QnMediaServerResource::apiConnection()
{
    return m_restConnection;
}

QnResourcePtr QnMediaServerResourceFactory::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    Q_UNUSED(resourceTypeId)

    QnResourcePtr result(new QnMediaServerResource());
    result->deserialize(parameters);

    return result;
}

QnAbstractStorageResourceList QnMediaServerResource::getStorages() const
{
    return m_storages;
}

void QnMediaServerResource::setStorages(const QnAbstractStorageResourceList &storages)
{
    m_storages = storages;
}

// --------------------------------------------------

/*
class TestConnectionTask: public QRunnable
{
public:
    TestConnectionTask(QnMediaServerResourcePtr owner, const QUrl& url): m_owner(owner), m_url(url) {}

    void run()
    {
        QnHTTPRawResponse response;
        QnSessionManager::instance()->sendGetRequest(m_url.toString(), QLatin1String("ping"), QnRequestHeaderList(), QnRequestParamList(), response);
        QByteArray guid = m_owner->getGuid().toUtf8();
        if (response.data.contains("Requested method is absent") || response.data.contains(guid))
        {
            // server OK
            m_owner->setPrimaryIF(m_url.host());
        }
    }
private:
    QnMediaServerResourcePtr m_owner;
    QUrl m_url;
};
*/

void QnMediaServerResource::at_pingResponse(QnHTTPRawResponse response, int responseNum)
{
    QByteArray guid = getGuid().toUtf8();
    if (response.data.contains("Requested method is absent") || response.data.contains(guid) || response.data.contains("<time><clock>"))
    {
        QMutexLocker lock(&m_mutex);

        // server OK
        QString urlStr = m_runningIfRequests.value(responseNum);
        if (urlStr == QLatin1String("proxy"))
            setPrimaryIF(urlStr);
        else
            setPrimaryIF(QUrl(urlStr).host());
        m_runningIfRequests.remove(responseNum);

        if(m_runningIfRequests.isEmpty() && m_guard) {
            m_guard->deleteLater();
            m_guard = NULL;
        }
    }
}

void QnMediaServerResource::setPrimaryIF(const QString& primaryIF)
{
    QMutexLocker lock(&m_mutex);
    if (m_primaryIFSelected)
        return;
    m_primaryIFSelected = true;
    
    QUrl origApiUrl = getApiUrl();

    if (primaryIF != QLatin1String("proxy"))
    {
        QUrl apiUrl(getApiUrl());
        apiUrl.setHost(primaryIF);
        setApiUrl(apiUrl.toString());

        QUrl url(getUrl());
        url.setHost(primaryIF);
        setUrl(url.toString());
    }

    m_primaryIf = primaryIF;
    emit serverIfFound(::toSharedPointer(this), primaryIF, origApiUrl.toString());
}

QString QnMediaServerResource::getPrimaryIF() const 
{
    QMutexLocker lock(&m_mutex);

    return m_primaryIf;
}

void QnMediaServerResource::setReserve(bool reserve)
{
    m_reserve = reserve;
}

bool QnMediaServerResource::getReserve() const
{
    return m_reserve;
}

QnMediaServerResource::PanicMode QnMediaServerResource::getPanicMode() const {
    return m_panicMode;
}

void QnMediaServerResource::setPanicMode(PanicMode panicMode) {
    if(m_panicMode == panicMode)
        return;

    m_panicMode = panicMode;

    emit panicModeChanged(::toSharedPointer(this)); // TODO: emit it AFTER mutex unlock.
}

void QnMediaServerResource::determineOptimalNetIF()
{
    QMutexLocker lock(&m_mutex);
    //if (m_prevNetAddrList == m_netAddrList)
    //    return;
    m_prevNetAddrList = m_netAddrList;
    m_primaryIFSelected = false;

    for (int i = 0; i < m_netAddrList.size(); ++i)
    {
        QUrl url(m_apiUrl);
        url.setHost(m_netAddrList[i].toString());
        //TestConnectionTask *task = new TestConnectionTask(toSharedPointer().dynamicCast<QnMediaServerResource>(), url);
        //QThreadPool::globalInstance()->start(task);
        int requestNum = QnSessionManager::instance()->sendAsyncGetRequest(url, QLatin1String("ping"), this, SLOT(at_pingResponse(QnHTTPRawResponse, int)), Qt::DirectConnection);
        m_runningIfRequests.insert(requestNum, url.toString());
    }

    // send request via proxy (ping request always send directly, other request are sent via proxy here)
    QnSleep::msleep(5); // send request slighty latter to preffer direct connect
    int requestNum = QnSessionManager::instance()->sendAsyncGetRequest(QUrl(m_apiUrl), QLatin1String("gettime"), this, SLOT(at_pingResponse(QnHTTPRawResponse, int)), Qt::DirectConnection);
    m_runningIfRequests.insert(requestNum, QLatin1String("proxy"));

    if(!m_guard) {
        m_guard = new QnMediaServerResourceGuard(::toSharedPointer(this));
        m_guard->moveToThread(qApp->thread());
    }
}

void QnMediaServerResource::updateInner(QnResourcePtr other) 
{
    QMutexLocker lock(&m_mutex);

    QnResource::updateInner(other);
    bool netAddrListChanged = false;

    QnMediaServerResourcePtr localOther = other.dynamicCast<QnMediaServerResource>();
    if(localOther) {
        setPanicMode(localOther->getPanicMode());

        m_reserve = localOther->m_reserve;
        netAddrListChanged = m_netAddrList != localOther->m_netAddrList;
        m_netAddrList = localOther->m_netAddrList;
        setApiUrl(localOther->m_apiUrl);
        m_streamingUrl = localOther->getStreamingUrl();

        QnAbstractStorageResourceList otherStorages = localOther->getStorages();
        
        // keep index uhcnaged (app server does not provide such info
        foreach(QnAbstractStorageResourcePtr storage, m_storages)
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
    if (netAddrListChanged)
        determineOptimalNetIF();
}

QString QnMediaServerResource::getProxyHost() const
{
    return m_restConnection->getProxyHost();
}

int QnMediaServerResource::getProxyPort() const
{
    return m_restConnection->getProxyPort();
}

QString QnMediaServerResource::getVersion() const
{
    return m_version;
}

void QnMediaServerResource::setVersion(const QString &version)
{
    m_version = version;
}
