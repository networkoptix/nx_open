#include "media_server_resource.h"

#include <QtCore/QUrl>
#include "utils/common/delete_later.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "api/session_manager.h"

QnLocalMediaServerResource::QnLocalMediaServerResource()
    : QnResource()
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
    m_panicMode(false)
{
    setTypeId(qnResTypePool->getResourceTypeId(QString(), QLatin1String("Server")));
    addFlags(QnResource::server | QnResource::remote);
    removeFlags(QnResource::media); // TODO: is this call needed here?
    setName(tr("Server"));

    m_primaryIFSelected = false;
}

QnMediaServerResource::~QnMediaServerResource()
{
    //delete m_rtspListener;
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
        m_restConnection = QnMediaServerConnectionPtr(new QnMediaServerConnection(restUrl), &qnDeleteLater);
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

class QnEmptyDataProvider: public QnAbstractMediaStreamDataProvider{
public:
    QnEmptyDataProvider(QnResourcePtr resource): QnAbstractMediaStreamDataProvider(resource){}

protected:
    virtual QnAbstractMediaDataPtr getNextData() override {
        return QnAbstractMediaDataPtr(new QnAbstractMediaData(0U, 1U));
    }
};

QnAbstractStreamDataProvider* QnMediaServerResource::createDataProviderInternal(ConnectionRole ){
    return new QnEmptyDataProvider(toSharedPointer());
}

// --------------------------------------------------

class TestConnectionTask: public QRunnable
{
public:
    TestConnectionTask(QnMediaServerResource* owner, const QUrl& url): m_owner(owner), m_url(url) {}

    void run()
    {
        QByteArray reply;
        QByteArray errorString;
        QnSessionManager::instance()->sendGetRequest(m_url.toString(), QLatin1String("ping"), QnRequestParamList(), reply, errorString);
        if (reply.contains("Requested method is absent"))
        {
            // server OK
            m_owner->setPrimaryIF(m_url.host());
        }
    }
private:
    QnMediaServerResource* m_owner;
    QUrl m_url;
};

void QnMediaServerResource::setPrimaryIF(const QString& primaryIF)
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

    m_primaryIf = primaryIF;
    emit serverIFFound(primaryIF);
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

bool QnMediaServerResource::isPanicMode() const {
    return m_panicMode;
}

void QnMediaServerResource::setPanicMode(bool panicMode) {
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
        TestConnectionTask *task = new TestConnectionTask(this, url);
        QThreadPool::globalInstance()->start(task);
    }
}

void QnMediaServerResource::updateInner(QnResourcePtr other) 
{
    QMutexLocker lock(&m_mutex);

    QnResource::updateInner(other);

    QnMediaServerResourcePtr localOther = other.dynamicCast<QnMediaServerResource>();
    if(localOther) {
        setPanicMode(localOther->isPanicMode());

        m_reserve = localOther->m_reserve;
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
