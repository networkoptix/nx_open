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
#include "utils/common/util.h"
#include "media_server_user_attributes.h"
#include "../resource_management/resource_pool.h"
#include "utils/serialization/lexical.h"
#include "core/resource/security_cam_resource.h"
#include "nx_ec/ec_proto_version.h"


class QnMediaServerResourceGuard: public QObject {
public:
    QnMediaServerResourceGuard(const QnMediaServerResourcePtr &resource): m_resource(resource) {}

private:
    QnMediaServerResourcePtr m_resource;
};

QnMediaServerResource::QnMediaServerResource(const QnResourceTypePool* resTypePool):
    m_serverFlags(Qn::SF_None)
{
    setTypeId(resTypePool->getFixedResourceTypeId(lit("Server")));
    addFlags(Qn::server | Qn::remote);
    removeFlags(Qn::media); // TODO: #Elric is this call needed here?

    //TODO: #GDM #EDGE in case of EDGE servers getName should return name of its camera. Possibly name just should be synced on Server.
    QnResource::setName(tr("Server"));

    m_statusTimer.restart();

    QnResourceList resList = qnResPool->getResourcesByParentId(getId()).filtered<QnSecurityCamResource>();
    if (!resList.isEmpty())
        m_firstCamera = resList.first();

    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnMediaServerResource::onNewResource, Qt::DirectConnection);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnMediaServerResource::onRemoveResource, Qt::DirectConnection);
}

QnMediaServerResource::~QnMediaServerResource()
{
    QMutexLocker lock(&m_mutex);
    m_runningIfRequests.clear();
}

void QnMediaServerResource::onNewResource(const QnResourcePtr &resource)
{
    QMutexLocker lock(&m_mutex);
    if (m_firstCamera.isNull() && resource.dynamicCast<QnSecurityCamResource>() &&  resource->getParentId() == getId())
        m_firstCamera = resource;
}

void QnMediaServerResource::onRemoveResource(const QnResourcePtr &resource)
{
    QMutexLocker lock(&m_mutex);
    if (m_firstCamera && resource->getId() == m_firstCamera->getId())
        m_firstCamera.clear();
}


QString QnMediaServerResource::getUniqueId() const
{
    assert(!getId().isNull());
    return QLatin1String("Server ") + getId().toString();
}

QString QnMediaServerResource::getName() const
{
    if (getServerFlags() & Qn::SF_Edge)
    {
        QMutexLocker lock(&m_mutex);
        if (m_firstCamera)
            return m_firstCamera->getName();
    }

    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    if( !(*lk)->name.isEmpty() )
        return (*lk)->name;
    return QnResource::getName();
}

void QnMediaServerResource::setName( const QString& name )
{
    QString oldName = getName();
    if (oldName == name)
        return;

    setServerName(name);
    emit nameChanged(toSharedPointer(this));
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
        if (m_restConnection)
            m_restConnection->setUrl(m_apiUrl);
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

QList<QHostAddress> QnMediaServerResource::getNetAddrList() const
{
    QMutexLocker lock(&m_mutex);
    return m_netAddrList;
}

void QnMediaServerResource::setAdditionalUrls(const QList<QUrl> &urls)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_additionalUrls == urls)
            return;
        m_additionalUrls = urls;
    }
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<QUrl> QnMediaServerResource::getAdditionalUrls() const
{
    QMutexLocker lock(&m_mutex);
    return m_additionalUrls;
}

void QnMediaServerResource::setIgnoredUrls(const QList<QUrl> &urls)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_ignoredUrls == urls)
            return;
        m_ignoredUrls = urls;
    }
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<QUrl> QnMediaServerResource::getIgnoredUrls() const
{
    QMutexLocker lock(&m_mutex);
    return m_ignoredUrls;
}

quint16 QnMediaServerResource::getPort() const {
    QUrl url(getApiUrl());
    return url.port(DEFAULT_APPSERVER_PORT);
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

void QnMediaServerResource::setPrimaryIF(const QString& primaryIF)
{
    QUrl origApiUrl = getApiUrl();
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex );
    
        QUrl apiUrl(getApiUrl());
        apiUrl.setHost(primaryIF);
        setApiUrl(apiUrl.toString());

        QUrl url(getUrl());
        url.setHost(primaryIF);
        setUrl(url.toString());
    }
    emit serverIfFound(::toSharedPointer(this), primaryIF, origApiUrl.toString());
}

Qn::PanicMode QnMediaServerResource::getPanicMode() const 
{
    QString strVal = getProperty(lit("panic_mode"));
    Qn::PanicMode result = Qn::PM_None;
    QnLexical::deserialize(strVal, &result);
    return result;
}

void QnMediaServerResource::setPanicMode(Qn::PanicMode panicMode) {
    if(getPanicMode() == panicMode)
        return;
    QString strVal;
    QnLexical::serialize(panicMode, &strVal);
    setProperty(lit("panic_mode"), strVal);
}

Qn::ServerFlags QnMediaServerResource::getServerFlags() const
{
    return m_serverFlags;
}

void QnMediaServerResource::setServerFlags(Qn::ServerFlags flags)
{
    m_serverFlags = flags;
}

QnAbstractStorageResourcePtr QnMediaServerResource::getStorageByUrl(const QString& url) const
{
   for(const QnAbstractStorageResourcePtr& storage: getStorages()) {
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
        if (m_version != localOther->m_version)
            modifiedFields << "versionChanged";

        m_serverFlags = localOther->m_serverFlags;
        netAddrListChanged = m_netAddrList != localOther->m_netAddrList;
        m_netAddrList = localOther->m_netAddrList;
        m_version = localOther->m_version;
        m_systemInfo = localOther->m_systemInfo;
        m_systemName = localOther->m_systemName;

        /*
        QnAbstractStorageResourceList otherStorages = localOther->getStorages();
        
        // Keep indices unchanged (Server does not provide this info).
        for(const QnAbstractStorageResourcePtr &storage: m_storages)
        {
            for(const QnAbstractStorageResourcePtr &otherStorage: otherStorages)
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
    if (netAddrListChanged || getPort() != localOther->getPort()) {
        m_apiUrl = localOther->m_apiUrl;    // do not update autodetected value with side changes
        if (m_restConnection)
            m_restConnection->setUrl(m_apiUrl);
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
    QnModuleInformation moduleInformation;
    moduleInformation.type = nxMediaServerId;
    moduleInformation.customization = QnAppInfo::customizationName();
    moduleInformation.sslAllowed = false;
    moduleInformation.protoVersion = getProperty(lit("protoVersion")).toInt();
    if (moduleInformation.protoVersion == 0)
        moduleInformation.protoVersion = nx_ec::EC2_PROTO_VERSION;
    
    QMutexLocker lock(&m_mutex);

    moduleInformation.version = m_version;
    moduleInformation.systemInformation = m_systemInfo;
    moduleInformation.systemName = m_systemName;
    moduleInformation.name = getName();
    moduleInformation.port = QUrl(m_apiUrl).port();
    for (const QHostAddress &address: m_netAddrList)
        moduleInformation.remoteAddresses.insert(address.toString());
    moduleInformation.id = getId();

    return moduleInformation;
}

bool QnMediaServerResource::isEdgeServer(const QnResourcePtr &resource) {
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return (server->getServerFlags() & Qn::SF_Edge);
    return false;
}

bool QnMediaServerResource::isHiddenServer(const QnResourcePtr &resource) {
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return (server->getServerFlags() & Qn::SF_Edge) && !server->isRedundancy();
    return false;
}

void QnMediaServerResource::setStatus(Qn::ResourceStatus newStatus, bool silenceMode)
{
    if (getStatus() != newStatus) 
    {
        {
            QMutexLocker lock(&m_mutex);
            m_statusTimer.restart();
        }

        QnResource::setStatus(newStatus, silenceMode);
        if (!silenceMode) 
        {
            QnResourceList childList = qnResPool->getResourcesByParentId(getId());
            for(const QnResourcePtr& res: childList) 
            {
                if (res->hasFlags(Qn::depend_on_parent_status))
                    emit res->statusChanged(res);
            }
        }
    }
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
