#include "media_server_resource.h"

#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include "api/session_manager.h"
#include <api/app_server_connection.h>
#include <api/model/ping_reply.h>
#include <api/network_proxy_factory.h>

#include <core/resource/storage_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/server_backup_schedule.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/resource_pool.h>

#include "nx_ec/ec_proto_version.h"

#include <rest/server/json_rest_result.h>

#include <utils/common/app_info.h>
#include "utils/common/delete_later.h"
#include "utils/common/sleep.h"
#include "utils/common/util.h"
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/socket_global.h>
#include "network/networkoptixmodulerevealcommon.h"
#include "nx/fusion/serialization/lexical.h"
#include "api/server_rest_connection.h"
#include <common/common_module.h>
#include <api/global_settings.h>

namespace {
    const QString kUrlScheme = lit("rtsp");
    const QString protoVersionPropertyName = lit("protoVersion");
    const QString safeModePropertyName = lit("ecDbReadOnly");
}

QString QnMediaServerResource::apiUrlScheme(bool sslAllowed)
{
    return sslAllowed ? lit("https") : lit("http");
}

QnMediaServerResource::QnMediaServerResource():
    base_type(),
    m_serverFlags(Qn::SF_None),
    m_panicModeCache(
        std::bind(&QnMediaServerResource::calculatePanicMode, this),
        &m_mutex )
{
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kServerTypeId));
    addFlags(Qn::server | Qn::remote);
    removeFlags(Qn::media); // TODO: #Elric is this call needed here?

    m_statusTimer.restart();

    QnResourceList resList = qnResPool->getResourcesByParentId(getId()).filtered<QnSecurityCamResource>();
    if (!resList.isEmpty())
        m_firstCamera = resList.first();

    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnMediaServerResource::onNewResource, Qt::DirectConnection);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnMediaServerResource::onRemoveResource, Qt::DirectConnection);
    connect(this, &QnResource::resourceChanged, this, &QnMediaServerResource::atResourceChanged, Qt::DirectConnection);
    connect(this, &QnResource::propertyChanged, this, &QnMediaServerResource::at_propertyChanged, Qt::DirectConnection);
}

QnMediaServerResource::~QnMediaServerResource()
{
    directDisconnectAll();
    QnMutexLocker lock( &m_mutex );
    m_runningIfRequests.clear();
}

void QnMediaServerResource::at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key)
{
    if (key == QnMediaResource::panicRecordingKey())
        m_panicModeCache.update();
}

void QnMediaServerResource::onNewResource(const QnResourcePtr &resource)
{
    QnMutexLocker lock( &m_mutex );
    if (m_firstCamera.isNull() && resource.dynamicCast<QnSecurityCamResource>() &&  resource->getParentId() == getId())
        m_firstCamera = resource;
}

void QnMediaServerResource::onRemoveResource(const QnResourcePtr &resource)
{
    QnMutexLocker lock( &m_mutex );
    if (m_firstCamera && resource->getId() == m_firstCamera->getId())
        m_firstCamera.clear();
}

void QnMediaServerResource::beforeDestroy()
{
    QnMutexLocker lock(&m_mutex);
    m_firstCamera.clear();
}

void QnMediaServerResource::atResourceChanged()
{
    m_panicModeCache.update();
}

QString QnMediaServerResource::getUniqueId() const
{
    NX_ASSERT(!getId().isNull());
    return QLatin1String("Server ") + getId().toString();
}

QString QnMediaServerResource::getName() const
{
    if (getServerFlags() & Qn::SF_Edge)
    {
        QnMutexLocker lock( &m_mutex );
        if (m_firstCamera)
            return m_firstCamera->getName();
    }

    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
        if( !(*lk)->name.isEmpty() )
            return (*lk)->name;
    }
    return QnResource::getName();
}

void QnMediaServerResource::setName( const QString& name )
{
    if (getId().isNull())
    {
        base_type::setName(name);
        return;
    }

    if (getServerFlags() & Qn::SF_Edge)
        return;

    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
        if ((*lk)->name == name)
            return;
        (*lk)->name = name;
    }
    emit nameChanged(toSharedPointer(this));
}

void QnMediaServerResource::setNetAddrList(const QList<SocketAddress>& netAddrList)
{
    {
        QnMutexLocker lock( &m_mutex );
        m_netAddrList = netAddrList;
    }
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<SocketAddress> QnMediaServerResource::getNetAddrList() const
{
    QnMutexLocker lock( &m_mutex );
    return m_netAddrList;
}

void QnMediaServerResource::setAdditionalUrls(const QList<QUrl> &urls)
{
    QnUuid id = getId();
    QList<QUrl> oldUrls = qnServerAdditionalAddressesDictionary->additionalUrls(id);
    if (oldUrls == urls)
        return;

    qnServerAdditionalAddressesDictionary->setAdditionalUrls(id, urls);
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<QUrl> QnMediaServerResource::getAdditionalUrls() const
{
    return qnServerAdditionalAddressesDictionary->additionalUrls(getId());
}

void QnMediaServerResource::setIgnoredUrls(const QList<QUrl> &urls)
{
    QnUuid id = getId();
    QList<QUrl> oldUrls = qnServerAdditionalAddressesDictionary->ignoredUrls(id);
    if (oldUrls == urls)
        return;

    qnServerAdditionalAddressesDictionary->setIgnoredUrls(id, urls);
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<QUrl> QnMediaServerResource::getIgnoredUrls() const
{
    return qnServerAdditionalAddressesDictionary->ignoredUrls(getId());
}

quint16 QnMediaServerResource::getPort() const
{
    return getPrimaryAddress().port;
}

QList<SocketAddress> QnMediaServerResource::getAllAvailableAddresses() const
{
    auto toAddress = [](const QUrl& url) { return SocketAddress(url.host(), url.port(0)); };

    QSet<SocketAddress> result;
    QSet<SocketAddress> ignored;
    for (const QUrl &url : getIgnoredUrls())
        ignored.insert(toAddress(url));

    for (const auto& address : getNetAddrList())
    {
        if (ignored.contains(address))
            continue;
        result.insert(address);
    }

    for (const QUrl &url : getAdditionalUrls())
    {
        SocketAddress address = toAddress(url);
        if (ignored.contains(address))
            continue;
        result.insert(address);
    }

    return result.toList();
}

QnMediaServerConnectionPtr QnMediaServerResource::apiConnection()
{
    QnMutexLocker lock(&m_mutex);

    /* We want the video server connection to be deleted in its associated thread,
     * no matter where the reference count reached zero. Hence the custom deleter. */
    if (!m_apiConnection)
    {
        QnMediaServerResourcePtr thisPtr = toSharedPointer(this).dynamicCast<QnMediaServerResource>();
        m_apiConnection = QnMediaServerConnectionPtr(
            new QnMediaServerConnection(thisPtr, QnAppServerConnectionFactory::videowallGuid()),
            &qnDeleteLater);
    }

    return m_apiConnection;
}

rest::QnConnectionPtr QnMediaServerResource::restConnection()
{
    QnMutexLocker lock( &m_mutex );

    if (!m_restConnection)
        m_restConnection = rest::QnConnectionPtr(new rest::ServerConnection(getId()));

    return m_restConnection;
}

void QnMediaServerResource::setUrl(const QString& url)
{
    QnResource::setUrl(url);

    QnMutexLocker lock(&m_mutex);
    if (!m_primaryAddress.isNull())
        return;

    if (m_apiConnection)
        m_apiConnection->setUrl(getApiUrl());

    lock.unlock();

    emit apiUrlChanged(toSharedPointer(this));
}

QUrl QnMediaServerResource::getApiUrl() const
{
    return getPrimaryAddress().toUrl(apiUrlScheme(isSslAllowed()));
}

QString QnMediaServerResource::getUrl() const
{
    return getPrimaryAddress().toUrl(kUrlScheme).toString();
}

QnStorageResourceList QnMediaServerResource::getStorages() const
{
    return qnResPool->getResourcesByParentId(getId()).filtered<QnStorageResource>();
}

void QnMediaServerResource::setPrimaryAddress(const SocketAddress& primaryAddress)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (primaryAddress == m_primaryAddress)
            return;

        m_primaryAddress = primaryAddress;
        if (m_apiConnection)
            m_apiConnection->setUrl(m_primaryAddress.toUrl(apiUrlScheme(m_sslAllowed)));
    }

    emit primaryAddressChanged(toSharedPointer(this));
}

bool QnMediaServerResource::isSslAllowed() const
{
    QnMutexLocker lock(&m_mutex);
    return m_sslAllowed;
}

void QnMediaServerResource::setSslAllowed(bool sslAllowed)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (sslAllowed == m_sslAllowed)
            return;

        m_sslAllowed = sslAllowed;
        if (m_apiConnection)
        {
            auto url = m_primaryAddress.isNull()
                ? m_apiConnection->url()
                : m_primaryAddress.toUrl(apiUrlScheme(m_sslAllowed));
            url.setScheme(apiUrlScheme(m_sslAllowed));
            m_apiConnection->setUrl(url);
        }
    }

    emit primaryAddressChanged(toSharedPointer(this));
    emit sslAllowedChanged(toSharedPointer(this));
}

SocketAddress QnMediaServerResource::getPrimaryAddress() const
{
    QnMutexLocker lock(&m_mutex);
    if (!m_primaryAddress.isNull())
        return m_primaryAddress;
    return SocketAddress(QUrl(m_url));
}

Qn::PanicMode QnMediaServerResource::getPanicMode() const
{
    return m_panicModeCache.get();
}

Qn::PanicMode QnMediaServerResource::calculatePanicMode() const
{
    QString strVal = getProperty(QnMediaResource::panicRecordingKey());
    Qn::PanicMode result = Qn::PM_None;
    QnLexical::deserialize(strVal, &result);
    return result;
}

void QnMediaServerResource::setPanicMode(Qn::PanicMode panicMode) {
    if(getPanicMode() == panicMode)
        return;
    QString strVal;
    QnLexical::serialize(panicMode, &strVal);
    setProperty(QnMediaResource::panicRecordingKey(), strVal);
    m_panicModeCache.update();
}

Qn::ServerFlags QnMediaServerResource::getServerFlags() const
{
    return m_serverFlags;
}

void QnMediaServerResource::setServerFlags(Qn::ServerFlags flags)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (flags == m_serverFlags)
            return;
        m_serverFlags = flags;
    }
    emit serverFlagsChanged(::toSharedPointer(this));
}

QnStorageResourcePtr QnMediaServerResource::getStorageByUrl(const QString& url) const
{
   for(const QnStorageResourcePtr& storage: getStorages()) {
       if (storage->getUrl() == url)
           return storage;
   }
   return QnStorageResourcePtr();
}

void QnMediaServerResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    /* Calculate primary address before the url is changed. */
    const SocketAddress oldPrimaryAddress = getPrimaryAddress();

    base_type::updateInternal(other, notifiers);

    QnMediaServerResource* localOther = dynamic_cast<QnMediaServerResource*>(other.data());
    if (localOther)
    {
        if (m_version != localOther->m_version)
        {
            m_version = localOther->m_version;
            notifiers << [r = toSharedPointer(this)]{ emit r->versionChanged(r); };
        }

        m_serverFlags = localOther->m_serverFlags;
        m_netAddrList = localOther->m_netAddrList;
        m_systemInfo = localOther->m_systemInfo;
        m_authKey = localOther->m_authKey;

    }

    const auto currentAddress = getPrimaryAddress();
    if (oldPrimaryAddress != currentAddress)
    {
        if (m_apiConnection)
            m_apiConnection->setUrl(getApiUrl());
        if (oldPrimaryAddress.port != currentAddress.port)
            notifiers << [r = toSharedPointer(this)]{ emit r->portChanged(r); };
    }
}

QnSoftwareVersion QnMediaServerResource::getVersion() const
{
    QnMutexLocker lock( &m_mutex );

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

QnServerBackupSchedule QnMediaServerResource::getBackupSchedule() const {
    QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
    return (*lk)->backupSchedule;
}

void QnMediaServerResource::setBackupSchedule(const QnServerBackupSchedule &value) {
    {
        QnMediaServerUserAttributesPool::ScopedLock lk( QnMediaServerUserAttributesPool::instance(), getId() );
        if ((*lk)->backupSchedule == value)
            return;
        (*lk)->backupSchedule = value;
    }
    emit backupScheduleChanged(::toSharedPointer(this));
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
        QnMutexLocker lock( &m_mutex );
        if (m_version == version)
            return;
        m_version = version;
    }
    emit versionChanged(::toSharedPointer(this));
}

QnSystemInformation QnMediaServerResource::getSystemInfo() const {
    QnMutexLocker lock( &m_mutex );

    return m_systemInfo;
}

void QnMediaServerResource::setSystemInfo(const QnSystemInformation &systemInfo) {
    QnMutexLocker lock( &m_mutex );

    m_systemInfo = systemInfo;
}
QnModuleInformation QnMediaServerResource::getModuleInformation() const
{
    if (getId() == qnCommon->moduleGUID())
        return qnCommon->moduleInformation();

    // build module information for other server

    QnModuleInformation moduleInformation;
    moduleInformation.type = QnModuleInformation::nxMediaServerId();
    moduleInformation.customization = QnAppInfo::customizationName();
    moduleInformation.sslAllowed = false;
    moduleInformation.protoVersion = getProperty(protoVersionPropertyName).toInt();
    moduleInformation.name = getName();
    if (moduleInformation.protoVersion == 0)
        moduleInformation.protoVersion = nx_ec::EC2_PROTO_VERSION;

    if (hasProperty(safeModePropertyName))
        moduleInformation.ecDbReadOnly = QnLexical::deserialized(getProperty(safeModePropertyName), moduleInformation.ecDbReadOnly);

    moduleInformation.systemName = qnGlobalSettings->systemName();
    moduleInformation.id = getId();
    moduleInformation.port = getPort();
    moduleInformation.version = getVersion();
    moduleInformation.systemInformation = getSystemInfo();
    moduleInformation.serverFlags = getServerFlags();

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
            QnMutexLocker lock( &m_mutex );
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
    QnMutexLocker lock( &m_mutex );
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

QString QnMediaServerResource::realm() const
{
    return QnAppInfo::realm();
}
