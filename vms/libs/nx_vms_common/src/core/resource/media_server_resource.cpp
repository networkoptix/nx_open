// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_server_resource.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <api/global_settings.h>
#include <api/model/ping_reply.h>
#include <api/network_proxy_factory.h>
#include <api/runtime_info_manager.h>
#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/edge/edge_server_state_tracker.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <nx/analytics/properties.h>
#include <nx/branding.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/delete_later.h>
#include <utils/common/sleep.h>
#include <utils/common/util.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/socket_global.h>

using namespace nx;

namespace {

const QString protoVersionPropertyName = lit("protoVersion");
const bool kWebCamerasDiscoveryEnabledDefaultValue = false;

} // namespace

const QString QnMediaServerResource::kMetadataStorageIdKey = "metadataStorageId";

QnMediaServerResource::QnMediaServerResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_serverFlags(vms::api::SF_None),
    m_panicModeCache(std::bind(&QnMediaServerResource::calculatePanicMode, this))
{
    setTypeId(nx::vms::api::MediaServerData::kResourceTypeId);
    addFlags(Qn::server | Qn::remote);
    removeFlags(Qn::media); // TODO: #sivanov Is this call needed here?

    m_statusTimer.restart();

    connect(this, &QnResource::resourceChanged,
        this, &QnMediaServerResource::resetCachedValues, Qt::DirectConnection);

    connect(this, &QnResource::propertyChanged,
        this, &QnMediaServerResource::at_propertyChanged, Qt::DirectConnection);
}

QnMediaServerResource::~QnMediaServerResource()
{
    directDisconnectAll();
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_runningIfRequests.clear();
}

void QnMediaServerResource::at_propertyChanged(const QnResourcePtr& /*res*/, const QString& key)
{
    if (key == QnMediaResource::panicRecordingKey())
    {
        m_panicModeCache.reset();
    }
    else if (key == ResourcePropertyKey::Server::kCertificate)
    {
        emit certificateChanged(toSharedPointer(this));
    }
    else if (key == ResourcePropertyKey::Server::kUserProvidedCertificate)
    {
        emit certificateChanged(toSharedPointer(this));
    }
    else if (key == ResourcePropertyKey::Server::kWebCamerasDiscoveryEnabled)
    {
        emit webCamerasDiscoveryChanged();
    }
    else if (key == nx::analytics::kDescriptorsProperty)
    {
        emit analyticsDescriptorsChanged();
    }
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
    if (getServerFlags().testFlag(vms::api::SF_Edge) && !m_edgeServerStateTracker.isNull())
    {
        if (const auto camera = m_edgeServerStateTracker->uniqueCoupledChildCamera())
            return camera->getName();
    }

    {
        QnMediaServerUserAttributesPool::ScopedLock lk(
            commonModule()->mediaServerUserAttributesPool(), getId());

        if (!lk->name().isEmpty())
            return lk->name();
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
        if (lk->name() == name)
            return;
        lk->setName(name);
    }
    emit nameChanged(toSharedPointer(this));
}

void QnMediaServerResource::setNetAddrList(const QList<nx::network::SocketAddress>& netAddrList)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_netAddrList == netAddrList)
            return;
        m_netAddrList = netAddrList;
    }
    emit auxUrlsChanged(::toSharedPointer(this));
}

QList<nx::network::SocketAddress> QnMediaServerResource::getNetAddrList() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
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

std::optional<nx::network::SocketAddress> QnMediaServerResource::getCloudAddress() const
{
    const auto cloudId = getModuleInformation().cloudId();
    if (cloudId.isEmpty())
        return std::nullopt;
    else
        return nx::network::SocketAddress(cloudId.toStdString());
}

quint16 QnMediaServerResource::getPort() const
{
    return getPrimaryAddress().port;
}

QList<nx::network::SocketAddress> QnMediaServerResource::getAllAvailableAddresses() const
{
    auto toAddress =
        [](const nx::utils::Url& url)
        {
            return nx::network::url::getEndpoint(url, 0);
        };

    QSet<nx::network::SocketAddress> ignored;
    for (const nx::utils::Url &url : getIgnoredUrls())
        ignored.insert(toAddress(url));

    QSet<nx::network::SocketAddress> result;
    for (const auto& address : getNetAddrList())
    {
        if (ignored.contains(address))
            continue;
        NX_ASSERT(!address.toString().empty());
        result.insert(address);
    }

    for (const nx::utils::Url& url : getAdditionalUrls())
    {
        nx::network::SocketAddress address = toAddress(url);
        if (ignored.contains(address))
            continue;
        NX_ASSERT(!address.toString().empty());
        result.insert(address);
    }

    if (auto cloudAddress = getCloudAddress())
    {
        NX_ASSERT(!cloudAddress->toString().empty());
        result.insert(std::move(*cloudAddress));
    }

    return result.values();
}

bool QnMediaServerResource::hasInternetAccess() const
{
    return getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP)
        && !hasFlags(Qn::fake)
        && getStatus() == nx::vms::api::ResourceStatus::online;
}

rest::ServerConnectionPtr QnMediaServerResource::restConnection()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!m_restConnection)
        m_restConnection = rest::ServerConnectionPtr(new rest::ServerConnection(
            resourcePool()->commonModule(),
            getId()));

    return m_restConnection;
}

void QnMediaServerResource::setUrl(const QString& url)
{
    QnResource::setUrl(url);

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_primaryAddress.isNull())
            return;
    }

    emit primaryAddressChanged(toSharedPointer(this));
    emit apiUrlChanged(toSharedPointer(this));
}

nx::utils::Url QnMediaServerResource::getApiUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::urlScheme(isSslAllowed()))
        .setEndpoint(getPrimaryAddress()).toUrl();
}

QString QnMediaServerResource::getUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::urlScheme(isSslAllowed()))
        .setEndpoint(getPrimaryAddress()).toUrl().toString();
}

bool QnMediaServerResource::isGuidConflictDetected() const
{
    const auto value = getProperty(ResourcePropertyKey::Server::kGuidConflictDetected);
    return QJson::deserialized<bool>(value.toUtf8());
}

void QnMediaServerResource::setGuidConflictDetected(bool value)
{
    setProperty(ResourcePropertyKey::Server::kGuidConflictDetected,
        QString::fromUtf8(QJson::serialized(value)));
}

QString QnMediaServerResource::rtspUrl() const
{
    bool isSecure = true;

    auto connection = commonModule()->ec2Connection();
    if (connection)
    {
        isSecure = commonModule()->globalSettings()->isVideoTrafficEncryptionForced()
            || connection->credentials().authToken.isBearerToken();
    }

    nx::network::url::Builder urlBuilder(getUrl());
    urlBuilder.setScheme(nx::network::rtsp::urlScheme(isSecure));
    return urlBuilder.toUrl().toString();
}

QnStorageResourceList QnMediaServerResource::getStorages() const
{
    auto result = resourcePool()->getResourcesByParentId(getId()).filtered<QnStorageResource>();
    for (const auto& s: resourcePool()->getResources<QnStorageResource>())
    {
        if (s->getParentId().isNull()) //< Cloud, system-wide storage.
            result.append(s);
    }

    return result;
}

void QnMediaServerResource::setPrimaryAddress(const nx::network::SocketAddress& primaryAddress)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (primaryAddress == m_primaryAddress)
            return;

        m_primaryAddress = primaryAddress;
        NX_ASSERT(!m_primaryAddress.address.toString().empty());
    }

    emit primaryAddressChanged(toSharedPointer(this));
}

bool QnMediaServerResource::isSslAllowed() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return nx::utils::Url(m_url).scheme() != nx::network::http::urlScheme(false)
        || commonModule()->globalSettings()->isTrafficEncryptionForced();
}

void QnMediaServerResource::setSslAllowed(bool sslAllowed)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (sslAllowed == isSslAllowed())
            return;

        nx::utils::Url url(m_url);
        url.setScheme(nx::network::http::urlScheme(sslAllowed));
        m_url = url.toString();
    }

    emit primaryAddressChanged(toSharedPointer(this));
}

nx::network::SocketAddress QnMediaServerResource::getPrimaryAddress() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
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
    const QString str = getProperty(QnMediaResource::panicRecordingKey());
    NX_DEBUG(this, "%1 calculated panic mode %2", getId(), str);
    return nx::reflect::fromString(str.toStdString(), Qn::PM_None);
}

void QnMediaServerResource::setPanicMode(Qn::PanicMode panicMode)
{
    if (getPanicMode() == panicMode)
        return;

    const QString str = QString::fromStdString(nx::reflect::toString(panicMode));
    NX_DEBUG(this, "%1 change panic mode to %2", getId(), str);
    setProperty(QnMediaResource::panicRecordingKey(), str);
    m_panicModeCache.update();
}

vms::api::ServerFlags QnMediaServerResource::getServerFlags() const
{
    return m_serverFlags;
}

void QnMediaServerResource::setServerFlags(vms::api::ServerFlags flags)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
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
        m_authKey = localOther->m_authKey;
        m_utcOffset = localOther->m_utcOffset.load();

    }

    const auto currentAddress = getPrimaryAddress();
    if (oldPrimaryAddress != currentAddress)
        notifiers << [r = toSharedPointer(this)]{ emit r->primaryAddressChanged(r); };
}

nx::utils::SoftwareVersion QnMediaServerResource::getVersion() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_version;
}

void QnMediaServerResource::setMaxCameras(int value)
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
    lk->setMaxCameras(value);
}

int QnMediaServerResource::getMaxCameras() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
    return lk->maxCameras();
}

void QnMediaServerResource::setLocationId(int value)
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId());
    lk->setLocationId(value);
}

int QnMediaServerResource::locationId() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId());
    return lk->locationId();
}

QnMediaServerUserAttributesPtr QnMediaServerResource::userAttributes() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId());
    return *lk;
}

QnUuid QnMediaServerResource::metadataStorageId() const
{
    return QnUuid::fromStringSafe(getProperty(kMetadataStorageIdKey));
}

void QnMediaServerResource::setMetadataStorageId(const QnUuid& value)
{
    setProperty(kMetadataStorageIdKey, value.toString());
}

void QnMediaServerResource::setRedundancy(bool value)
{
    {
        QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
        if (lk->isRedundancyEnabled() == value)
            return;
        lk->setIsRedundancyEnabled(value);
    }
    emit redundancyChanged(::toSharedPointer(this));
}

bool QnMediaServerResource::isRedundancy() const
{
    QnMediaServerUserAttributesPool::ScopedLock lk(commonModule()->mediaServerUserAttributesPool(), getId() );
    return lk->isRedundancyEnabled();
}

void QnMediaServerResource::setCompatible(bool value)
{
    if (m_isCompatible == value)
        return;

    m_isCompatible = value;
    emit compatibilityChanged(::toSharedPointer(this));
}

bool QnMediaServerResource::isCompatible() const
{
    return m_isCompatible;
}

void QnMediaServerResource::setVersion(const nx::utils::SoftwareVersion& version)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_version == version)
            return;
        m_version = version;
    }
    emit versionChanged(::toSharedPointer(this));
}

utils::OsInfo QnMediaServerResource::getOsInfo() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_osInfo;
}

void QnMediaServerResource::setOsInfo(const utils::OsInfo& osInfo)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_osInfo = osInfo;
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
    moduleInformation.type = nx::vms::api::ModuleInformation::mediaServerId();
    moduleInformation.customization = nx::branding::customization();
    moduleInformation.sslAllowed = isSslAllowed();
    moduleInformation.realm = nx::network::AppInfo::realm().c_str();
    moduleInformation.cloudHost = nx::network::SocketGlobals::cloud().cloudHost().c_str();
    moduleInformation.name = getName();
    moduleInformation.protoVersion = getProperty(protoVersionPropertyName).toInt();
    if (moduleInformation.protoVersion == 0)
        moduleInformation.protoVersion = nx::vms::api::protocolVersion();

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
    moduleInformation.osInfo = getOsInfo();
    moduleInformation.serverFlags = getServerFlags();
    if (moduleInformation.isNewSystem())
        moduleInformation.serverFlags |= nx::vms::api::SF_NewSystem; //< Legacy API compatibility.

    return moduleInformation;
}

nx::vms::api::ModuleInformationWithAddresses
    QnMediaServerResource::getModuleInformationWithAddresses() const
{
    nx::vms::api::ModuleInformationWithAddresses information = getModuleInformation();
    ec2::setModuleInformationEndpoints(information, getAllAvailableAddresses());
    return information;
}

bool QnMediaServerResource::isEdgeServer(const QnResourcePtr& resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return (server->getServerFlags().testFlag(vms::api::SF_Edge));
    return false;
}

nx::core::resource::edge::EdgeServerStateTracker* QnMediaServerResource::edgeServerStateTracker()
{
    return m_edgeServerStateTracker.get();
}

bool QnMediaServerResource::isArmServer(const QnResourcePtr& resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return (server->getServerFlags().testFlag(vms::api::SF_ArmServer));
    return false;
}

bool QnMediaServerResource::isHiddenEdgeServer(const QnResourcePtr& resource)
{
    // EDGE server may be represented as single camera item if:
    // 1. It have no child cameras except single non-virtual one with the same host address.
    // 2. Redundancy is set off.

    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
    {
        if (!server->getServerFlags().testFlag(vms::api::SF_Edge) || server->isRedundancy())
            return false;

        if (server->edgeServerStateTracker())
            return server->edgeServerStateTracker()->hasCanonicalState();
    }
    return false;
}

void QnMediaServerResource::setStatus(
    nx::vms::api::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_statusTimer.restart();
    }

    QnResource::setStatus(newStatus, reason);
    if (auto pool = resourcePool())
    {
        QnResourceList childList = pool->getResourcesByParentId(getId());
        for (const QnResourcePtr& res: childList)
        {
            if (res->hasFlags(Qn::depend_on_parent_status))
            {
                NX_VERBOSE(this, "%1 Emit statusChanged signal for resource %2, %3, %4",
                    __func__, res->getId(), res->getName(), res->getUrl());
                emit res->statusChanged(res, Qn::StatusChangeReason::Local);
            }
        }
    }
}

qint64 QnMediaServerResource::currentStatusTime() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_statusTimer.elapsed();
}

qint64 QnMediaServerResource::utcOffset(qint64 defaultValue) const
{
    const qint64 value = m_utcOffset;
    return value == Qn::InvalidUtcOffset ? defaultValue : value;
}

void QnMediaServerResource::setUtcOffset(qint64 value)
{
    m_utcOffset = value;
}

QString QnMediaServerResource::getAuthKey() const
{
    return m_authKey;
}

void QnMediaServerResource::setAuthKey(const QString& authKey)
{
    m_authKey = authKey;
}

nx::vms::api::BackupBitrateBytesPerSecond QnMediaServerResource::getBackupBitrateLimitsBps() const
{
    QnMediaServerUserAttributesPool::ScopedLock lock(
        commonModule()->mediaServerUserAttributesPool(), getId());
    return lock->backupBitrateBytesPerSecond();
}

void QnMediaServerResource::setBackupBitrateLimitsBps(
    const nx::vms::api::BackupBitrateBytesPerSecond& bitrateLimits)
{
    {
        QnMediaServerUserAttributesPool::ScopedLock lock(
            commonModule()->mediaServerUserAttributesPool(), getId());

        if (lock->backupBitrateBytesPerSecond() == bitrateLimits)
            return;

        lock->setBackupBitrateBytesPerSecond(bitrateLimits);
    }
    emit backupBitrateLimitsChanged(::toSharedPointer(this));
}

QString QnMediaServerResource::realm() const
{
    return nx::network::AppInfo::realm().c_str();
}

void QnMediaServerResource::setResourcePool(QnResourcePool* resourcePool)
{
    if (auto oldPool = this->resourcePool())
    {
        oldPool->disconnect(this);
        oldPool->commonModule()->globalSettings()->disconnect(this);
    }

    base_type::setResourcePool(resourcePool);

    if (!resourcePool)
        return;

    if (getServerFlags().testFlag(vms::api::SF_Edge))
    {
        using namespace nx::core::resource::edge;
        m_edgeServerStateTracker.reset(new EdgeServerStateTracker(this));

        connect(edgeServerStateTracker(), &EdgeServerStateTracker::hasCoupledCameraChanged,
            this, [this] { emit this->nameChanged(toSharedPointer(this)); });

        connect(edgeServerStateTracker(), &EdgeServerStateTracker::hasCanonicalStateChanged,
            this, [this] { emit this->edgeServerCanonicalStateChanged(toSharedPointer(this)); });
    }

    const auto& settings = resourcePool->commonModule()->globalSettings();
    connect(settings, &QnGlobalSettings::cloudSettingsChanged,
        this, &QnMediaServerResource::at_cloudSettingsChanged, Qt::DirectConnection);
}

std::string QnMediaServerResource::certificate() const
{
    return getProperty(ResourcePropertyKey::Server::kCertificate).toStdString();
}

std::string QnMediaServerResource::userProvidedCertificate() const
{
    return getProperty(ResourcePropertyKey::Server::kUserProvidedCertificate).toStdString();
}

bool QnMediaServerResource::isWebCamerasDiscoveryEnabled() const
{
    return QnLexical::deserialized(
        getProperty(ResourcePropertyKey::Server::kWebCamerasDiscoveryEnabled),
        kWebCamerasDiscoveryEnabledDefaultValue);
}

void QnMediaServerResource::setWebCamerasDiscoveryEnabled(bool value)
{
    setProperty(ResourcePropertyKey::Server::kWebCamerasDiscoveryEnabled,
        value == kWebCamerasDiscoveryEnabledDefaultValue ? QString() : QnLexical::serialized(value)
    );
}
