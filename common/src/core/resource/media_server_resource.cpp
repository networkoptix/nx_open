#include "media_server_resource.h"

#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include <utils/common/app_info.h>
#include "utils/common/delete_later.h"
#include "api/session_manager.h"
#include <api/app_server_connection.h>
#include <api/model/ping_reply.h>
#include <api/network_proxy_factory.h>
#include <rest/server/json_rest_result.h>
#include "utils/common/sleep.h"
#include "utils/network/networkoptixmodulerevealcommon.h"
#include "media_server_user_attributes.h"
#include "../resource_management/resource_pool.h"


const QString QnMediaServerResource::USE_PROXY = QLatin1String("proxy");

class QnMediaServerResourceGuard: public QObject {
public:
    QnMediaServerResourceGuard(const QnMediaServerResourcePtr &resource): m_resource(resource) {}

private:
    QnMediaServerResourcePtr m_resource;
};

QnMediaServerResource::QnMediaServerResource(const QnResourceTypePool* resTypePool):
    m_primaryIFSelected(false),
    m_serverFlags(Qn::SF_None),
    m_panicMode(Qn::PM_None),
    m_guard(NULL)
{
    setTypeId(resTypePool->getFixedResourceTypeId(lit("Server")));
    addFlags(Qn::server | Qn::remote);
    removeFlags(Qn::media); // TODO: #Elric is this call needed here?

    //TODO: #GDM #EDGE in case of EDGE servers getName should return name of its camera. Possibly name just should be synced on Server.
    QnResource::setName(tr("Server"));

    m_primaryIFSelected = false;
    m_statusTimer.restart();
}

QnMediaServerResource::~QnMediaServerResource()
{
    QMutexLocker lock(&m_mutex); // TODO: #Elric remove once m_guard hell is removed.
}

QString QnMediaServerResource::getUniqueId() const
{
    assert(!getId().isNull());
    return QLatin1String("Server ") + getId().toString();
}

QString QnMediaServerResource::getName() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    if( !(*lk)->name.isEmpty() )
        return (*lk)->name;
    return QnResource::getName();
}

void QnMediaServerResource::setName( const QString& name )
{
    setServerName( name );
    QnResource::setName( name );
}

void QnMediaServerResource::setServerName( const QString& name )
{
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    (*lk)->name = name;
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

void QnMediaServerResource::setNetAddrList(const QList<QHostAddress>& netAddrList)
{
    QMutexLocker lock(&m_mutex);
    m_netAddrList = netAddrList;
}

const QList<QHostAddress>& QnMediaServerResource::getNetAddrList() const
{
    QMutexLocker lock(&m_mutex);
    return m_netAddrList;
}

void QnMediaServerResource::setAdditionalUrls(const QList<QUrl> &urls)
{
    QMutexLocker lock(&m_mutex);
    m_additionalUrls = urls;
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<QUrl> QnMediaServerResource::getAdditionalUrls() const
{
    QMutexLocker lock(&m_mutex);
    return m_additionalUrls;
}

void QnMediaServerResource::setIgnoredUrls(const QList<QUrl> &urls)
{
    QMutexLocker lock(&m_mutex);
    m_ignoredUrls = urls;
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<QUrl> QnMediaServerResource::getIgnoredUrls() const
{
    QMutexLocker lock(&m_mutex);
    return m_ignoredUrls;
}

QnMediaServerConnectionPtr QnMediaServerResource::apiConnection()
{
    QMutexLocker lock(&m_mutex);

    /* We want the video server connection to be deleted in its associated thread, 
     * no matter where the reference count reached zero. Hence the custom deleter. */
    if (!m_restConnection && !m_apiUrl.isEmpty())
        m_restConnection = QnMediaServerConnectionPtr(new QnMediaServerConnection(this, QnAppServerConnectionFactory::videowallGuid()), &qnDeleteLater);

    return m_restConnection;
}

QnResourcePtr QnMediaServerResourceFactory::createResource(const QnUuid& resourceTypeId, const QnResourceParams& /*params*/)
{
    Q_UNUSED(resourceTypeId)

    QnResourcePtr result(new QnMediaServerResource(qnResTypePool));
    //result->deserialize(parameters);

    return result;
}

QnAbstractStorageResourceList QnMediaServerResource::getStorages() const
{
    return qnResPool->getResourcesByParentId(getId()).filtered<QnAbstractStorageResource>();
}

void QnMediaServerResource::setStorageDataToUpdate(const QnAbstractStorageResourceList& storagesToUpdate, const ec2::ApiIdDataList& storagesToRemove)
{
    m_storagesToUpdate = storagesToUpdate;
    m_storagesToRemove = storagesToRemove;
}

QPair<int, int> QnMediaServerResource::saveUpdatedStorages()
{
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    int updateHandle = conn->getMediaServerManager()->saveStorages(m_storagesToUpdate, this, &QnMediaServerResource::onRequestDone);
    int removeHandle = conn->getMediaServerManager()->removeStorages(m_storagesToRemove, this, &QnMediaServerResource::onRequestDone);

    return QPair<int, int>(updateHandle, removeHandle);
}

void QnMediaServerResource::onRequestDone( int reqID, ec2::ErrorCode errorCode )
{
    emit storageSavingDone(reqID, errorCode);
}

/*
void QnMediaServerResource::setStorages(const QnAbstractStorageResourceList &storages)
{
    m_storages = storages;
}
*/

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

    const QString& urlStr = m_runningIfRequests.value(responseNum);
    if( response.status == QNetworkReply::NoError )
    {
        QnPingReply reply;
        QnJsonRestResult result;
        if( QJson::deserialize(response.data, &result) && QJson::deserialize(result.reply(), &reply) && (reply.moduleGuid == getId()) )
        {
            // server OK
            if (urlStr == QnMediaServerResource::USE_PROXY)
                setPrimaryIF(urlStr);
            else
                setPrimaryIF(QUrl(urlStr).host());
        }
    }

    m_runningIfRequests.remove(responseNum);

    if(m_runningIfRequests.isEmpty() && m_guard) {
        m_guard->deleteLater();
        m_guard = NULL;
    }
}

void QnMediaServerResource::setPrimaryIF(const QString& primaryIF)
{
    QUrl origApiUrl = getApiUrl();
    {
        QMutexLocker lock(&m_mutex);
        if (m_primaryIFSelected)
            return;
        m_primaryIFSelected = true;
    
        if (primaryIF != USE_PROXY)
        {
            QUrl apiUrl(getApiUrl());
            apiUrl.setHost(primaryIF);
            setApiUrl(apiUrl.toString());

            QUrl url(getUrl());
            url.setHost(primaryIF);
            setUrl(url.toString());
        }

        m_primaryIf = primaryIF;
    }
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
    {
        QMutexLocker lock(&m_mutex);
        if(m_panicMode == panicMode)
            return;
        m_panicMode = panicMode;
    }
    emit panicModeChanged(::toSharedPointer(this));
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
    
    //using proxy before we able to establish direct connection
    setPrimaryIF( QnMediaServerResource::USE_PROXY );

    m_prevNetAddrList = m_netAddrList;
    m_primaryIFSelected = false;

    for (int i = 0; i < m_netAddrList.size(); ++i)
    {
        QUrl url(m_apiUrl);
        url.setHost(m_netAddrList[i].toString());
        int requestNum = QnSessionManager::instance()->sendAsyncGetRequest(url, QLatin1String("ping"), this, "at_pingResponse", Qt::DirectConnection);
        m_runningIfRequests.insert(requestNum, url.toString());
    }

    if(!m_guard) {
        m_guard = new QnMediaServerResourceGuard(::toSharedPointer(this));
        m_guard->moveToThread(qApp->thread());
    }
}

QnAbstractStorageResourcePtr QnMediaServerResource::getStorageByUrl(const QString& url) const
{
   foreach(const QnAbstractStorageResourcePtr& storage, getStorages()) {
       if (storage->getUrl() == url)
           return storage;
   }
   return QnAbstractStorageResourcePtr();
}

void QnMediaServerResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) {
    QString oldUrl = m_url;
    QnResource::updateInner(other, modifiedFields);
    bool netAddrListChanged = false;

    QnMediaServerResource* localOther = dynamic_cast<QnMediaServerResource*>(other.data());
    if(localOther) {
        if (m_panicMode != localOther->m_panicMode)
            modifiedFields << "panicModeChanged";
        if (m_version != localOther->m_version)
            modifiedFields << "versionChanged";

        m_panicMode = localOther->m_panicMode;

        m_serverFlags = localOther->m_serverFlags;
        netAddrListChanged = m_netAddrList != localOther->m_netAddrList;
        m_netAddrList = localOther->m_netAddrList;
        m_version = localOther->m_version;
        m_systemInfo = localOther->m_systemInfo;
        m_systemName = localOther->m_systemName;

        /*
        QnAbstractStorageResourceList otherStorages = localOther->getStorages();
        
        // Keep indices unchanged (Server does not provide this info).
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
        */
    }
    if (netAddrListChanged) {
        m_apiUrl = localOther->m_apiUrl;    // do not update autodetected value with side changes
        determineOptimalNetIF();
    } else {
        m_url = oldUrl; //rollback changed value to autodetected
    }
}

QnSoftwareVersion QnMediaServerResource::getVersion() const
{
    QMutexLocker lock(&m_mutex);

    return m_version;
}

void QnMediaServerResource::setMaxCameras(int value)
{
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    (*lk)->maxCameras = value;
}

int QnMediaServerResource::getMaxCameras() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    return (*lk)->maxCameras;
}

void QnMediaServerResource::setRedundancy(bool value)
{
    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
        if ((*lk)->isRedundancyEnabled == value)
            return;
        (*lk)->isRedundancyEnabled = value;
    }
    emit redundancyChanged(::toSharedPointer(this));
}

bool QnMediaServerResource::isRedundancy() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    return (*lk)->isRedundancyEnabled;
}

void QnMediaServerResource::setVersion(const QnSoftwareVersion &version)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_version == version)
            return;
        m_version = version;
    }
    emit versionChanged(::toSharedPointer(this));
}

QnSystemInformation QnMediaServerResource::getSystemInfo() const {
    QMutexLocker lock(&m_mutex);

    return m_systemInfo;
}

void QnMediaServerResource::setSystemInfo(const QnSystemInformation &systemInfo) {
    QMutexLocker lock(&m_mutex);

    m_systemInfo = systemInfo;
}

QString QnMediaServerResource::getSystemName() const {
    QMutexLocker lock(&m_mutex);

    return m_systemName;
}

void QnMediaServerResource::setSystemName(const QString &systemName) {
    {
        QMutexLocker lock(&m_mutex);

        if (m_systemName == systemName)
            return;

        m_systemName = systemName;
    }
    emit systemNameChanged(toSharedPointer(this));
}

QnModuleInformation QnMediaServerResource::getModuleInformation() const {
    QMutexLocker lock(&m_mutex);

    QnModuleInformation moduleInformation;
    moduleInformation.type = nxMediaServerId;
    moduleInformation.customization = QnAppInfo::customizationName();
    moduleInformation.version = m_version;
    moduleInformation.systemInformation = m_systemInfo;
    moduleInformation.systemName = m_systemName;
    moduleInformation.name = getName();
    moduleInformation.port = QUrl(m_apiUrl).port();
    foreach (const QHostAddress &address, m_netAddrList)
        moduleInformation.remoteAddresses.insert(address.toString());
    moduleInformation.id = getId();
    moduleInformation.sslAllowed = false;
    return moduleInformation;
}

bool QnMediaServerResource::isHiddenServer(const QnResourcePtr &resource) {
    if (QnMediaServerResource* server = dynamic_cast<QnMediaServerResource*>(resource.data())) 
        return (server->getServerFlags() & Qn::SF_Edge) && !server->isRedundancy();
    return false;
}

void QnMediaServerResource::setStatus(Qn::ResourceStatus newStatus, bool silenceMode)
{
    if (getStatus() != newStatus) {
        QMutexLocker lock(&m_mutex);
        m_statusTimer.restart();
    }
    QnResource::setStatus(newStatus, silenceMode);
}

qint64 QnMediaServerResource::currentStatusTime() const
{
    QMutexLocker lock(&m_mutex);
    return m_statusTimer.elapsed();
}

QString QnMediaServerResource::getAuthKey() const
{
    return m_authKey;
}

void QnMediaServerResource::setAuthKey(const QString& authKey)
{
    m_authKey = authKey;
}
