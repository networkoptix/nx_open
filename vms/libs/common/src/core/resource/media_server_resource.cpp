#include "media_server_resource.h"

#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include <api/session_manager.h>
#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/model/ping_reply.h>
#include <api/network_proxy_factory.h>
#include <api/runtime_info_manager.h>
#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/storage_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/server_backup_schedule.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/resource_pool.h>
#include <network/networkoptixmodulerevealcommon.h>
#include <nx_ec/ec_proto_version.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <rest/server/json_rest_result.h>
#include <utils/common/app_info.h>
#include <utils/common/delete_later.h>
#include <utils/common/sleep.h>
#include <utils/common/util.h>

#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

using namespace nx;

namespace {

const QString protoVersionPropertyName = lit("protoVersion");
const QString safeModePropertyName = lit("ecDbReadOnly");

} // namespace

QnMediaServerResource::QnMediaServerResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_serverFlags(vms::api::SF_None),
    m_panicModeCache(
        std::bind(&QnMediaServerResource::calculatePanicMode, this),
        &m_mutex )
{
    setTypeId(nx::vms::api::MediaServerData::kResourceTypeId);
    addFlags(Qn::server | Qn::remote);
    removeFlags(Qn::media); // TODO: #Elric is this call needed here?

    m_statusTimer.restart();

    connect(this, &QnResource::resourceChanged,
        this, &QnMediaServerResource::resetCachedValues, Qt::DirectConnection);

    connect(this, &QnResource::propertyChanged,
        this, &QnMediaServerResource::at_propertyChanged, Qt::DirectConnection);
}

QnMediaServerResource::~QnMediaServerResource()
{
    directDisconnectAll();
    QnMutexLocker lock(&m_mutex);
    m_runningIfRequests.clear();
}

void QnMediaServerResource::at_propertyChanged(const QnResourcePtr& /*res*/, const QString& key)
{
    if (key == QnMediaResource::panicRecordingKey())
        m_panicModeCache.reset();
}

void QnMediaServerResource::at_cloudSettingsChanged()
{
    if (hasFlags(Qn::fake_server))
        return;

    emit auxUrlsChanged(toSharedPointer(this));
}

void QnMediaServerResource::resetCachedValues()
{
    m_panicModeCache.reset();
}

void QnMediaServerResource::onNewResource(const QnResourcePtr &resource)
{
    QnMutexLocker lock(&m_mutex);
    if (m_firstCamera.isNull() && resource.dynamicCast<QnSecurityCamResource>() &&  resource->getParentId() == getId())
        m_firstCamera = resource;
}

void QnMediaServerResource::onRemoveResource(const QnResourcePtr &resource)
{
    QnMutexLocker lock(&m_mutex);
    if (m_firstCamera && resource->getId() == m_firstCamera->getId())
        m_firstCamera.clear();
}

void QnMediaServerResource::beforeDestroy()
{
    QnMutexLocker lock(&m_mutex);
    m_firstCamera.clear();
}

QSet<QnUuid> QnMediaServerResource::activeAnalyticsEngineIds() const
{
    const auto commonModule = this->commonModule();
    if (!NX_ASSERT(commonModule))
        return {};

    const auto runtimeInfoManager = commonModule->runtimeInfoManager();
    if (!NX_ASSERT(runtimeInfoManager))
        return {};

    const auto runtimeInfo = runtimeInfoManager->item(getId());
    return runtimeInfo.data.activeAnalyticsEngines;
}

QString QnMediaServerResource::getUniqueId() const
{
    NX_ASSERT(!getId().isNull());
    return QLatin1String("Server ") + getId().toString();
}

QString QnMediaServerResource::getName() const
{
    if (getServerFlags().testFlag(vms::api::SF_Edge))
    {
        QnMutexLocker lock(&m_mutex);
        if (m_firstCamera)
            return m_firstCamera->getName();
    }

    {
        QnMediaServerUserAttributesPool::ScopedLock lk(
            commonModule()->mediaServerUserAttributesPool(), getId());

        const QnMediaServerUserAttributesPtr userAttributes = *lk;

        if (!userAttributes->name.isEmpty())
            return userAttributes->name;
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

    if (getServerFlags().testFlag(vms::api::SF_Edge))
        return;

    {
        QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
        if ((*lk)->name == name)
            return;
        (*lk)->name = name;
    }
    emit nameChanged(toSharedPointer(this));
}

void QnMediaServerResource::setNetAddrList(const QList<nx::network::SocketAddress>& netAddrList)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_netAddrList == netAddrList)
            return;
        m_netAddrList = netAddrList;
    }
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<nx::network::SocketAddress> QnMediaServerResource::getNetAddrList() const
{
    QnMutexLocker lock(&m_mutex);
    return m_netAddrList;
}

void QnMediaServerResource::setAdditionalUrls(const QList<nx::utils::Url> &urls)
{
    QnUuid id = getId();
    QList<nx::utils::Url> oldUrls = commonModule()->serverAdditionalAddressesDictionary()->additionalUrls(id);
    if (oldUrls == urls)
        return;

    commonModule()->serverAdditionalAddressesDictionary()->setAdditionalUrls(id, urls);
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<nx::utils::Url> QnMediaServerResource::getAdditionalUrls() const
{
    return commonModule()->serverAdditionalAddressesDictionary()->additionalUrls(getId());
}

void QnMediaServerResource::setIgnoredUrls(const QList<nx::utils::Url> &urls)
{
    QnUuid id = getId();
    QList<nx::utils::Url> oldUrls = commonModule()->serverAdditionalAddressesDictionary()->ignoredUrls(id);
    if (oldUrls == urls)
        return;

    commonModule()->serverAdditionalAddressesDictionary()->setIgnoredUrls(id, urls);
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<nx::utils::Url> QnMediaServerResource::getIgnoredUrls() const
{
    return commonModule()->serverAdditionalAddressesDictionary()->ignoredUrls(getId());
}

boost::optional<nx::network::SocketAddress> QnMediaServerResource::getCloudAddress() const
{
    const auto cloudId = getModuleInformation().cloudId();
    if (cloudId.isEmpty())
        return boost::none;
    else
        return nx::network::SocketAddress(cloudId);
}

quint16 QnMediaServerResource::getPort() const
{
    return getPrimaryAddress().port;
}

QList<nx::network::SocketAddress> QnMediaServerResource::getAllAvailableAddresses() const
{
    auto toAddress = [](const nx::utils::Url& url) { return nx::network::SocketAddress(url.host(), url.port(0)); };

    QSet<nx::network::SocketAddress> ignored;
    for (const nx::utils::Url &url : getIgnoredUrls())
        ignored.insert(toAddress(url));

    QSet<nx::network::SocketAddress> result;
    for (const auto& address : getNetAddrList())
    {
        if (ignored.contains(address))
            continue;
        NX_ASSERT(!address.toString().isEmpty());
        result.insert(address);
    }

    for (const nx::utils::Url& url : getAdditionalUrls())
    {
        nx::network::SocketAddress address = toAddress(url);
        if (ignored.contains(address))
            continue;
        NX_ASSERT(!address.toString().isEmpty());
        result.insert(address);
    }

    if (auto cloudAddress = getCloudAddress())
    {
        NX_ASSERT(!cloudAddress->toString().isEmpty());
        result.insert(std::move(*cloudAddress));
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
            new QnMediaServerConnection(
                commonModule(),
                thisPtr,
                commonModule()->videowallGuid()),
                &qnDeleteLater);
    }

    return m_apiConnection;
}

rest::QnConnectionPtr QnMediaServerResource::restConnection()
{
    QnMutexLocker lock(&m_mutex);

    if (!m_restConnection)
        m_restConnection = rest::QnConnectionPtr(new rest::ServerConnection(
            resourcePool()->commonModule(),
            getId()));

    return m_restConnection;
}

void QnMediaServerResource::setUrl(const QString& url)
{
    QnResource::setUrl(url);

    {
        QnMutexLocker lock(&m_mutex);
        if (!m_primaryAddress.isNull())
            return;
    }

    emit primaryAddressChanged(toSharedPointer(this));
    emit apiUrlChanged(toSharedPointer(this));
}

nx::utils::Url QnMediaServerResource::getApiUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::urlSheme(isSslAllowed()))
        .setEndpoint(getPrimaryAddress()).toUrl();
}

QString QnMediaServerResource::getUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::urlSheme(isSslAllowed()))
        .setEndpoint(getPrimaryAddress()).toUrl().toString();
}

QString QnMediaServerResource::rtspUrl() const
{
    const auto isSecure = commonModule()->globalSettings()->isVideoTrafficEncriptionForced();
    nx::network::url::Builder urlBuilder(getUrl());
    urlBuilder.setScheme(nx::network::rtsp::urlSheme(isSecure));
    return urlBuilder.toString();
}

QnStorageResourceList QnMediaServerResource::getStorages() const
{
    return commonModule()->resourcePool()->getResourcesByParentId(getId()).filtered<QnStorageResource>();
}

void QnMediaServerResource::setPrimaryAddress(const nx::network::SocketAddress& primaryAddress)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (primaryAddress == m_primaryAddress)
            return;

        m_primaryAddress = primaryAddress;
        NX_ASSERT(!m_primaryAddress.address.toString().isEmpty());
    }

    emit primaryAddressChanged(toSharedPointer(this));
}

bool QnMediaServerResource::isSslAllowed() const
{
    QnMutexLocker lock(&m_mutex);
    return nx::utils::Url(m_url).scheme() != nx::network::http::urlSheme(false)
        || commonModule()->globalSettings()->isTrafficEncriptionForced();
}

void QnMediaServerResource::setSslAllowed(bool sslAllowed)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (sslAllowed == isSslAllowed())
            return;

        nx::utils::Url url(m_url);
        url.setScheme(nx::network::http::urlSheme(sslAllowed));
        m_url = url.toString();
    }

    emit primaryAddressChanged(toSharedPointer(this));
}

nx::network::SocketAddress QnMediaServerResource::getPrimaryAddress() const
{
    QnMutexLocker lock(&m_mutex);
    if (!m_primaryAddress.isNull())
        return m_primaryAddress;
    if (m_url.isEmpty())
        return nx::network::SocketAddress();
    return nx::network::url::getEndpoint(nx::utils::Url(m_url));
}

Qn::PanicMode QnMediaServerResource::getPanicMode() const
{
    return m_panicModeCache.get();
}

Qn::PanicMode QnMediaServerResource::calculatePanicMode() const
{
    QString strVal = getProperty(QnMediaResource::panicRecordingKey());
    NX_DEBUG(this, lm("%1 calculated panic mode %2").args(getId(), strVal));
    Qn::PanicMode result = Qn::PM_None;
    QnLexical::deserialize(strVal, &result);
    return result;
}

void QnMediaServerResource::setPanicMode(Qn::PanicMode panicMode) {
    if(getPanicMode() == panicMode)
        return;

    QString strVal;
    QnLexical::serialize(panicMode, &strVal);
    NX_DEBUG(this, lm("%1 change panic mode to %2").args(getId(), strVal));
    setProperty(QnMediaResource::panicRecordingKey(), strVal);
    m_panicModeCache.update();
}

vms::api::ServerFlags QnMediaServerResource::getServerFlags() const
{
    return m_serverFlags;
}

void QnMediaServerResource::setServerFlags(vms::api::ServerFlags flags)
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
    const nx::network::SocketAddress oldPrimaryAddress = getPrimaryAddress();
    const auto oldApiUrl = getUrl();

    base_type::updateInternal(other, notifiers);
    if (getUrl() != oldApiUrl)
        notifiers << [r = toSharedPointer(this)]{ emit r->apiUrlChanged(r); };

    QnMediaServerResource* localOther = dynamic_cast<QnMediaServerResource*>(other.data());
    if (localOther)
    {
        if (m_version != localOther->m_version)
            notifiers << [r = toSharedPointer(this)]{ emit r->versionChanged(r); };
        if (m_serverFlags != localOther->m_serverFlags)
            notifiers << [r = toSharedPointer(this)]{ emit r->serverFlagsChanged(r); };
        if (m_netAddrList != localOther->m_netAddrList)
            notifiers << [r = toSharedPointer(this)]{ emit r->auxUrlsChanged(r); };

        m_version = localOther->m_version;
        m_serverFlags = localOther->m_serverFlags;
        m_netAddrList = localOther->m_netAddrList;
        m_systemInfo = localOther->m_systemInfo;
        m_authKey = localOther->m_authKey;

    }

    const auto currentAddress = getPrimaryAddress();
    if (oldPrimaryAddress != currentAddress)
        notifiers << [r = toSharedPointer(this)]{ emit r->primaryAddressChanged(r); };
}

nx::utils::SoftwareVersion QnMediaServerResource::getVersion() const
{
    QnMutexLocker lock(&m_mutex);
    return m_version;
}

void QnMediaServerResource::setMaxCameras(int value)
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
    (*lk)->maxCameras = value;
}

int QnMediaServerResource::getMaxCameras() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
    return (*lk)->maxCameras;
}

QnMediaServerUserAttributesPtr QnMediaServerResource::userAttributes() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId());
    return *lk;
}

QnUuid QnMediaServerResource::metadataStorageId() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId());
    return (*lk)->metadataStorageId;
}

void QnMediaServerResource::setMetadataStorageId(const QnUuid& value)
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId());
    (*lk)->metadataStorageId = value;
}

QnServerBackupSchedule QnMediaServerResource::getBackupSchedule() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
    return (*lk)->backupSchedule;
}

void QnMediaServerResource::setBackupSchedule(const QnServerBackupSchedule &value) {
    {
        QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
        if ((*lk)->backupSchedule == value)
            return;
        (*lk)->backupSchedule = value;
    }
    emit backupScheduleChanged(::toSharedPointer(this));
}

void QnMediaServerResource::setRedundancy(bool value)
{
    {
        QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
        if ((*lk)->isRedundancyEnabled == value)
            return;
        (*lk)->isRedundancyEnabled = value;
    }
    emit redundancyChanged(::toSharedPointer(this));
}

bool QnMediaServerResource::isRedundancy() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
    return (*lk)->isRedundancyEnabled;
}

void QnMediaServerResource::setVersion(const nx::utils::SoftwareVersion& version)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_version == version)
            return;
        m_version = version;
    }
    emit versionChanged(::toSharedPointer(this));
}

nx::vms::api::SystemInformation QnMediaServerResource::getSystemInfo() const
{
    QnMutexLocker lock(&m_mutex);
    return m_systemInfo;
}

void QnMediaServerResource::setSystemInfo(const nx::vms::api::SystemInformation& systemInfo)
{
    QnMutexLocker lock(&m_mutex);
    m_systemInfo = systemInfo;
}

nx::vms::api::ModuleInformation QnMediaServerResource::getModuleInformation() const
{
    if (auto module = commonModule())
    {
        if (getId() == module->moduleGUID())
            return module->moduleInformation();
    }

    // build module information for other server

    nx::vms::api::ModuleInformation moduleInformation;
    moduleInformation.type = nx::vms::api::ModuleInformation::nxMediaServerId();
    moduleInformation.customization = QnAppInfo::customizationName();
    moduleInformation.sslAllowed = isSslAllowed();
    moduleInformation.realm = nx::network::AppInfo::realm();
    moduleInformation.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    moduleInformation.name = getName();
    moduleInformation.protoVersion = getProperty(protoVersionPropertyName).toInt();
    if (moduleInformation.protoVersion == 0)
        moduleInformation.protoVersion = nx_ec::EC2_PROTO_VERSION;

    if (hasProperty(safeModePropertyName))
    {
        moduleInformation.ecDbReadOnly = QnLexical::deserialized(
            getProperty(safeModePropertyName), moduleInformation.ecDbReadOnly);
    }

    if (auto module = commonModule())
    {
        const auto& settings = module->globalSettings();
        moduleInformation.localSystemId = settings->localSystemId();
        moduleInformation.systemName = settings->systemName();
        moduleInformation.cloudSystemId = settings->cloudSystemId();
    }

    moduleInformation.id = getId();
    moduleInformation.port = getPort();
    moduleInformation.version = getVersion();
    moduleInformation.systemInformation = getSystemInfo();
    moduleInformation.serverFlags = getServerFlags();

    return moduleInformation;
}

nx::vms::api::ModuleInformationWithAddresses
    QnMediaServerResource::getModuleInformationWithAddresses() const
{
    nx::vms::api::ModuleInformationWithAddresses information = getModuleInformation();
    ec2::setModuleInformationEndpoints(information, getAllAvailableAddresses());
    return information;
}

bool QnMediaServerResource::isEdgeServer(const QnResourcePtr &resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return (server->getServerFlags().testFlag(vms::api::SF_Edge));
    return false;
}

bool QnMediaServerResource::isArmServer(const QnResourcePtr &resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return (server->getServerFlags().testFlag(vms::api::SF_ArmServer));
    return false;
}

bool QnMediaServerResource::isHiddenServer(const QnResourcePtr &resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return server->getServerFlags().testFlag(vms::api::SF_Edge) && !server->isRedundancy();
    return false;
}

void QnMediaServerResource::setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    if (getStatus() != newStatus)
    {
        {
            QnMutexLocker lock(&m_mutex);
            m_statusTimer.restart();
        }

        QnResource::setStatus(newStatus, reason);
        QnResourceList childList = resourcePool()->getResourcesByParentId(getId());
        for(const QnResourcePtr& res: childList)
        {
            if (res->hasFlags(Qn::depend_on_parent_status))
            {
                NX_VERBOSE(this, lit("%1 Emit statusChanged signal for resource %2, %3, %4")
                        .arg(QString::fromLatin1(Q_FUNC_INFO))
                        .arg(res->getId().toString())
                        .arg(res->getName())
                        .arg(res->getUrl()));
                emit res->statusChanged(res, Qn::StatusChangeReason::Local);
            }
        }
    }
}

qint64 QnMediaServerResource::currentStatusTime() const
{
    QnMutexLocker lock(&m_mutex);
    return m_statusTimer.elapsed();
}

qint64 QnMediaServerResource::utcOffset(qint64 defaultValue) const
{
    bool present = true;
    const auto offset = getProperty(
        ResourcePropertyKey::Server::kTimezoneUtcOffset).toLongLong(&present);

    if (present && offset != Qn::InvalidUtcOffset)
        return offset;
    return defaultValue;
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
    return nx::network::AppInfo::realm();
}

void QnMediaServerResource::setResourcePool(QnResourcePool *resourcePool)
{
    if (auto oldPool = this->resourcePool())
    {
        oldPool->disconnect(this);
        oldPool->commonModule()->globalSettings()->disconnect(this);
        m_firstCamera.clear();
    }

    base_type::setResourcePool(resourcePool);

    if (auto pool = this->resourcePool())
    {
        if (getServerFlags().testFlag(vms::api::SF_Edge))
        {
            // watch to own camera to change default server name
            connect(pool, &QnResourcePool::resourceAdded,
                this, &QnMediaServerResource::onNewResource, Qt::DirectConnection);

            connect(pool, &QnResourcePool::resourceRemoved,
                this, &QnMediaServerResource::onRemoveResource, Qt::DirectConnection);

            QnResourceList resList = pool->getResourcesByParentId(getId()).filtered<QnSecurityCamResource>();
            if (!resList.isEmpty())
                m_firstCamera = resList.first();
        }

        const auto& settings = pool->commonModule()->globalSettings();
        connect(settings, &QnGlobalSettings::cloudSettingsChanged,
            this, &QnMediaServerResource::at_cloudSettingsChanged, Qt::DirectConnection);
    }
}
