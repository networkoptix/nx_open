#include "media_server_resource.h"

#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include "utils/common/delete_later.h"
#include "api/session_manager.h"
#include "utils/common/sleep.h"

class QnMediaServerResourceGuard: public QObject {
public:
    QnMediaServerResourceGuard(const QnMediaServerResourcePtr &resource): m_resource(resource) {}

private:
    QnMediaServerResourcePtr m_resource;
};

QnMediaServerResource::QnMediaServerResource(const QnResourceTypePool* resTypePool):
    QnResource(),
    m_panicMode(Qn::PM_None),
    m_guard(NULL),
    m_serverFlags(Qn::SF_None)
{
    setTypeId(resTypePool->getResourceTypeId(QString(), QLatin1String("Server")));
    addFlags(QnResource::server | QnResource::remote);
    removeFlags(QnResource::media); // TODO: #Elric is this call needed here?

      //TODO: #GDM in case of EDGE servers getName should return name of its camera. Possibly name just should be synced on EC.
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
    if (getId().isNull())
        nonConstThis->setId(QnId::createUuid());
    return QLatin1String("Server ") + getId().toString();
}

void QnMediaServerResource::setApiUrl(const QString& restUrl)
{
    QMutexLocker lock(&m_mutex);
    if (restUrl != m_apiUrl)
    {
        m_apiUrl = restUrl;
        m_restConnection.clear();
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
    QMutexLocker lock(&m_mutex);

    /* We want the video server connection to be deleted in its associated thread, 
     * no matter where the reference count reached zero. Hence the custom deleter. */
    if (!m_restConnection && !m_apiUrl.isEmpty())
        m_restConnection = QnMediaServerConnectionPtr(new QnMediaServerConnection(this), &qnDeleteLater);

    return m_restConnection;
}

QnResourcePtr QnMediaServerResourceFactory::createResource(QnId resourceTypeId, const QString& url)
{
    Q_UNUSED(resourceTypeId)

    QnResourcePtr result(new QnMediaServerResource(qnResTypePool));
    //result->deserialize(parameters);

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
    QMutexLocker lock(&m_mutex);

    QString urlStr = m_runningIfRequests.value(responseNum);
    QByteArray guid = getGuid().toByteArray();
    if (response.data.contains("Requested method is absent") || response.data.contains(guid) || response.data.contains("<time><clock>"))
    {
        // server OK
        if (urlStr == QLatin1String("proxy"))
            setPrimaryIF(urlStr);
        else
            setPrimaryIF(QUrl(urlStr).host());
    }

    m_runningIfRequests.remove(responseNum);

    if(m_runningIfRequests.isEmpty() && m_guard) {
        m_guard->deleteLater();
        m_guard = NULL;
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

Qn::PanicMode QnMediaServerResource::getPanicMode() const {
    return m_panicMode;
}

void QnMediaServerResource::setPanicMode(Qn::PanicMode panicMode) {
    if(m_panicMode == panicMode)
        return;

    m_panicMode = panicMode;

    emit panicModeChanged(::toSharedPointer(this)); // TODO: #Elric emit it AFTER mutex unlock.
}

Qn::ServerFlags QnMediaServerResource::getServerFlags() const
{
    return m_serverFlags;
}

void QnMediaServerResource::setServerFlags(Qn::ServerFlags flags)
{
    m_serverFlags = flags;
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
        //TestConnectionTask *task = new TestConnectionTask(::toSharedPointer(this), url);
        //QThreadPool::globalInstance()->start(task);
        int requestNum = QnSessionManager::instance()->sendAsyncGetRequest(url, QLatin1String("ping"), this, "at_pingResponse", Qt::DirectConnection);
        m_runningIfRequests.insert(requestNum, url.toString());
    }

    // send request via proxy (ping request always send directly, other request are sent via proxy here)
    QTimer *timer = new QTimer();
    timer->setSingleShot(true);
    connect(timer, SIGNAL(timeout()), this, SLOT(determineOptimalNetIF_testProxy()), Qt::DirectConnection);
    connect(timer, SIGNAL(timeout()), timer, SLOT(deleteLater()));
    timer->start(5); // send request slighty later to preffer direct connect // TODO: #Elric still bad, implement properly
    timer->moveToThread(qApp->thread());

    m_runningIfRequests.insert(-1, QString());

    if(!m_guard) {
        m_guard = new QnMediaServerResourceGuard(::toSharedPointer(this));
        m_guard->moveToThread(qApp->thread());
    }
}

void QnMediaServerResource::determineOptimalNetIF_testProxy() {
    QMutexLocker lock(&m_mutex);
    m_runningIfRequests.remove(-1);
    int requestNum = QnSessionManager::instance()->sendAsyncGetRequest(QUrl(m_apiUrl), QLatin1String("gettime"), this, "at_pingResponse", Qt::DirectConnection);
    m_runningIfRequests.insert(requestNum, QLatin1String("proxy"));
}

void QnMediaServerResource::updateInner(QnResourcePtr other) 
{
    QMutexLocker lock(&m_mutex);

    QnResource::updateInner(other);
    bool netAddrListChanged = false;

    QnMediaServerResourcePtr localOther = other.dynamicCast<QnMediaServerResource>();
    if(localOther) {
        setPanicMode(localOther->getPanicMode());

        m_serverFlags = localOther->m_serverFlags;
        netAddrListChanged = m_netAddrList != localOther->m_netAddrList;
        m_netAddrList = localOther->m_netAddrList;
        setApiUrl(localOther->m_apiUrl);
        m_streamingUrl = localOther->getStreamingUrl();
        m_version = localOther->getVersion();

        QnAbstractStorageResourceList otherStorages = localOther->getStorages();
        
        /* Keep indices unchanged (EC does not provide this info). */
        foreach(const QnAbstractStorageResourcePtr &storage, m_storages)
        {
            foreach(const QnAbstractStorageResourcePtr &otherStorage, otherStorages)
            {
                if (otherStorage->getId() == storage->getId()) {
                    otherStorage->setIndex(storage->getIndex());
                    break;
                }
            }
        }

        setStorages(otherStorages);
    }
    if (netAddrListChanged)
        determineOptimalNetIF();
}


QString QnMediaServerResource::getProxyHost()
{
    QnMediaServerConnectionPtr connection = apiConnection();
    return connection ? connection->getProxyHost() : QString();
}

int QnMediaServerResource::getProxyPort()
{
    QnMediaServerConnectionPtr connection = apiConnection();
    return connection ? connection->getProxyPort() : 0;
}

QnSoftwareVersion QnMediaServerResource::getVersion() const
{
    QMutexLocker lock(&m_mutex);

    return m_version;
}

void QnMediaServerResource::setVersion(const QnSoftwareVersion &version)
{
    QMutexLocker lock(&m_mutex);

    m_version = version;
}

bool QnMediaServerResource::isEdgeServer(const QnResourcePtr &resource) {
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>()) 
        return (server->getServerFlags() & Qn::SF_Edge);
    return false;
}
