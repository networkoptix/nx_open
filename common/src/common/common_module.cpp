#include "common_module.h"

#include <QSettings>
#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>

#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <api/common_message_processor.h>

#include <nx/vms/event/rule_manager.h>

#include <common/common_meta_types.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>
#include <core/resource_access/providers/shared_resource_access_provider.h>
#include <core/resource_access/providers/shared_layout_item_access_provider.h>
#include <core/resource_access/providers/videowall_item_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>

#include <licensing/license.h>

#include <utils/common/app_info.h>

#include <nx/network/socket_global.h>
#include <nx/vms/discovery/manager.h>

#include <api/session_manager.h>
#include <network/router.h>

#include <analytics/plugins/detection/detection_plugin_factory.h>
#include <analytics/common/metadata_plugin_factory.h>

using namespace nx;

namespace {

static const QString kAdminPasswordHash = lit("adminMd5Hash");
static const QString kAdminPasswordDigest = lit("adminMd5Digest");
static const QString kAdminPasswordCrypt512 = lit("adminCrypt512");
static const QString kAdminPasswordRealm = lit("adminRealm");
static const QString kLocalSystemId = lit("localSystemId");
static const QString kLocalSystemName = lit("localSystemName");
static const QString kServerName = lit("serverName");
static const QString kStorageInfo = lit("storageInfo");

} // namespace

void BeforeRestoreDbData::saveToSettings(QSettings* settings)
{
    settings->setValue(kAdminPasswordHash, hash);
    settings->setValue(kAdminPasswordDigest, digest);
    settings->setValue(kAdminPasswordCrypt512, cryptSha512Hash);
    settings->setValue(kAdminPasswordRealm, realm);
    settings->setValue(kLocalSystemId, localSystemId);
    settings->setValue(kLocalSystemName, localSystemName);
    settings->setValue(kServerName, serverName);
    settings->setValue(kStorageInfo, storageInfo);
}

void BeforeRestoreDbData::loadFromSettings(const QSettings* settings)
{
    hash = settings->value(kAdminPasswordHash).toByteArray();
    digest = settings->value(kAdminPasswordDigest).toByteArray();
    cryptSha512Hash = settings->value(kAdminPasswordCrypt512).toByteArray();
    realm = settings->value(kAdminPasswordRealm, nx::network::AppInfo::realm()).toByteArray();
    localSystemId = settings->value(kLocalSystemId).toByteArray();
    localSystemName = settings->value(kLocalSystemName).toByteArray();
    serverName = settings->value(kServerName).toByteArray();
    storageInfo = settings->value(kStorageInfo).toByteArray();
}

void BeforeRestoreDbData::clearSettings(QSettings* settings)
{
    settings->remove(kAdminPasswordHash);
    settings->remove(kAdminPasswordDigest);
    settings->remove(kAdminPasswordCrypt512);
    settings->remove(kAdminPasswordRealm);
    settings->remove(kLocalSystemId);
    settings->remove(kLocalSystemName);
    settings->remove(kServerName);
    settings->remove(kStorageInfo);
}

bool BeforeRestoreDbData::isEmpty() const
{
    return digest.isEmpty() && hash.isEmpty();
}

bool BeforeRestoreDbData::hasInfoForStorage(const QString& url) const
{
    return storageInfo.contains(url.toLocal8Bit());
}

qint64 BeforeRestoreDbData::getSpaceLimitForStorage(const QString& url) const
{
    int urlPos = storageInfo.indexOf(url);
    if (urlPos == -1)
        return -1;

    int spaceLimitStringBeginPos = urlPos + url.size() + 1;
    int spaceLimitStringEndPos = storageInfo.indexOf(";", spaceLimitStringBeginPos);

    return storageInfo.mid(spaceLimitStringBeginPos, spaceLimitStringEndPos - spaceLimitStringBeginPos).toLongLong();
}

// ------------------- QnCommonModule --------------------

QnCommonModule::QnCommonModule(bool clientMode,
    core::access::Mode resourceAccessMode,
    QObject* parent)
    :
    QObject(parent),
    m_messageProcessor(nullptr)
{
    m_dirtyModuleInformation = true;
    m_cloudMode = false;

    m_sessionManager = new QnSessionManager(this);

    m_licensePool = new QnLicensePool(this);
    m_cameraUserAttributesPool = new QnCameraUserAttributePool(this);
    m_mediaServerUserAttributesPool = new QnMediaServerUserAttributesPool(this);
    m_resourcePropertyDictionary = new QnResourcePropertyDictionary(this);
    m_resourceStatusDictionary = new QnResourceStatusDictionary(this);
    m_serverAdditionalAddressesDictionary = new QnServerAdditionalAddressesDictionary(this);

    m_resourcePool = new QnResourcePool(this);  /*< Depends on nothing. */
    m_layoutTourManager = new QnLayoutTourManager(this); //< Depends on nothing.
    m_eventRuleManager = new nx::vms::event::RuleManager(this); //< Depends on nothing.
    m_detectionPluginFactory = new nx::analytics::DetectionPluginFactory(this); //< Depends on nothing.
    m_metadataPluginFactory = new nx::analytics::MetadataPluginFactory(this); //< Depends on nothing.    
    m_runtimeInfoManager = new QnRuntimeInfoManager(this); //< Depends on nothing.

    // Depends on resource pool.
    m_moduleDiscoveryManager = new nx::vms::discovery::Manager(clientMode, this);
    // TODO: bind m_moduleDiscoveryManager to resPool server changes
    m_router = new QnRouter(this, m_moduleDiscoveryManager);

    m_userRolesManager = new QnUserRolesManager(this);         /*< Depends on nothing. */
    m_sharedResourceManager = new QnSharedResourcesManager(this);   /*< Depends on respool and roles. */

    // Depends on respool and roles.
    m_globalPermissionsManager = new QnGlobalPermissionsManager(resourceAccessMode, this);

    // Depends on respool, roles and global permissions.
    m_resourceAccessSubjectCache = new QnResourceAccessSubjectsCache(this);

    // Depends on respool, roles and shared resources.
    m_resourceAccessProvider = new QnResourceAccessProvider(resourceAccessMode, this);

    // Some of base providers depend on QnGlobalPermissionsManager and QnSharedResourcesManager.
    m_resourceAccessProvider->addBaseProvider(
        new QnPermissionsResourceAccessProvider(resourceAccessMode, this));
    m_resourceAccessProvider->addBaseProvider(
        new QnSharedResourceAccessProvider(resourceAccessMode, this));
    m_resourceAccessProvider->addBaseProvider(
        new QnSharedLayoutItemAccessProvider(resourceAccessMode, this));
    m_resourceAccessProvider->addBaseProvider(
        new QnVideoWallItemAccessProvider(resourceAccessMode, this));

    // Depends on access provider.
    m_resourceAccessManager = new QnResourceAccessManager(resourceAccessMode, this);

    m_globalSettings = new QnGlobalSettings(this);
    m_cameraHistory = new QnCameraHistoryPool(this);

    /* Init members. */
    m_runUuid = QnUuid::createUuid();
    m_startupTime = QDateTime::currentDateTime();

    m_moduleInformation.protoVersion = nx_ec::EC2_PROTO_VERSION;
    m_moduleInformation.systemInformation = QnSystemInformation::currentSystemInformation();
    m_moduleInformation.brand = QnAppInfo::productNameShort();
    m_moduleInformation.customization = QnAppInfo::customizationName();
    m_moduleInformation.version = QnSoftwareVersion(QnAppInfo::engineVersion());
    m_moduleInformation.type = clientMode ?
        QnModuleInformation::nxClientId() : QnModuleInformation::nxMediaServerId();
}

void QnCommonModule::setModuleGUID(const QnUuid& guid)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_uuid = guid;
    }
    resetCachedValue(); //< Update module information
}

QnCommonModule::~QnCommonModule()
{
    /* Here all singletons will be destroyed, so we guarantee all socket work will stop. */
    clear();
}

void QnCommonModule::bindModuleinformation(const QnMediaServerResourcePtr &server)
{
    /* Can't use resourceChanged signal because it's not emited when we are saving server locally. */
    connect(server.data(),  &QnMediaServerResource::nameChanged,    this,   &QnCommonModule::resetCachedValue);
    connect(server.data(),  &QnMediaServerResource::apiUrlChanged,  this,   &QnCommonModule::resetCachedValue);
    connect(server.data(),  &QnMediaServerResource::serverFlagsChanged,  this,   &QnCommonModule::resetCachedValue);
    connect(server.data(),  &QnMediaServerResource::primaryAddressChanged,  this,   &QnCommonModule::resetCachedValue);

    connect(m_globalSettings, &QnGlobalSettings::systemNameChanged, this, &QnCommonModule::resetCachedValue);
    connect(m_globalSettings, &QnGlobalSettings::localSystemIdChanged, this, &QnCommonModule::resetCachedValue);
    connect(m_globalSettings, &QnGlobalSettings::cloudSettingsChanged, this, &QnCommonModule::resetCachedValue);

    resetCachedValue();
}

void QnCommonModule::setRemoteGUID(const QnUuid &guid)
{
    {
        QnMutexLocker lock( &m_mutex );
        if (m_remoteUuid == guid)
            return;
        m_remoteUuid = guid;
    }
    emit remoteIdChanged(guid);
}

QnUuid QnCommonModule::remoteGUID() const {
    QnMutexLocker lock( &m_mutex );
    return m_remoteUuid;
}

QUrl QnCommonModule::currentUrl() const
{
    if (auto connection = ec2Connection())
        return connection->connectionInfo().ecUrl;
    return QUrl();
}

QnMediaServerResourcePtr QnCommonModule::currentServer() const
{
    QnUuid serverId = remoteGUID();
    if (serverId.isNull())
        return QnMediaServerResourcePtr();
    return m_resourcePool->getResourceById(serverId).dynamicCast<QnMediaServerResource>();
}

void QnCommonModule::setReadOnly(bool value)
{
    {
        QnMutexLocker lk(&m_mutex);
        if (m_moduleInformation.ecDbReadOnly == value)
            return;
        m_moduleInformation.ecDbReadOnly = value;
    }
    emit moduleInformationChanged();
    emit readOnlyChanged(value);
}

bool QnCommonModule::isReadOnly() const
{
    QnMutexLocker lk(&m_mutex);
    return m_moduleInformation.ecDbReadOnly;
}

void QnCommonModule::setModuleInformation(const QnModuleInformation& moduleInformation)
{
    bool isReadOnlyChanged = false;
    {
        QnMutexLocker lk( &m_mutex );
        if (m_moduleInformation == moduleInformation)
            return;

        isReadOnlyChanged = m_moduleInformation.ecDbReadOnly != moduleInformation.ecDbReadOnly;
        m_moduleInformation = moduleInformation;

        // TODO: This is generally a bad idea to fill some structure in setter and always replace
        //     some of the values by internal logic. It has to be redesigned.
        m_dirtyModuleInformation = true;
    }
    if (isReadOnlyChanged)
        emit readOnlyChanged(moduleInformation.ecDbReadOnly);
    emit moduleInformationChanged();
}

QnModuleInformation QnCommonModule::moduleInformation()
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_dirtyModuleInformation)
        {
            updateModuleInformationUnsafe();
            m_dirtyModuleInformation = false;
        }
        return m_moduleInformation;
    }
}

void QnCommonModule::resetCachedValue()
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_dirtyModuleInformation)
            return;
        m_dirtyModuleInformation = true;
    }
    emit moduleInformationChanged();
}

void QnCommonModule::updateModuleInformationUnsafe()
{
    // This code works only on server side.
    NX_ASSERT(!moduleGUID().isNull());
    m_moduleInformation.id = m_uuid;
    m_moduleInformation.runtimeId = m_runUuid;

    QnMediaServerResourcePtr server = m_resourcePool->getResourceById<QnMediaServerResource>(moduleGUID());
    //NX_ASSERT(server);
    if (!server)
        return;
    if (server->getPort())
        m_moduleInformation.port = server->getPort();
    m_moduleInformation.name = server->getName();
    m_moduleInformation.serverFlags = server->getServerFlags();

    m_moduleInformation.systemName = m_globalSettings->systemName();
    m_moduleInformation.localSystemId = m_globalSettings->localSystemId();
    m_moduleInformation.cloudSystemId = m_globalSettings->cloudSystemId();
}

void QnCommonModule::setSystemIdentityTime(qint64 value, const QnUuid& sender)
{
    m_systemIdentityTime = value;
    emit systemIdentityTimeChanged(value, sender);
}

qint64 QnCommonModule::systemIdentityTime() const
{
    return m_systemIdentityTime;
}

void QnCommonModule::setBeforeRestoreData(const BeforeRestoreDbData& data)
{
    m_beforeRestoreDbData = data;
}

BeforeRestoreDbData QnCommonModule::beforeRestoreDbData() const
{
    return m_beforeRestoreDbData;
}

void QnCommonModule::setUseLowPriorityAdminPasswordHack(bool value)
{
    m_lowPriorityAdminPassword = value;
}

bool QnCommonModule::useLowPriorityAdminPasswordHack() const
{
    return m_lowPriorityAdminPassword;
}

QnLicensePool* QnCommonModule::licensePool() const
{
    return m_licensePool;
}

QnUserRolesManager* QnCommonModule::userRolesManager() const
{
    return m_userRolesManager;
}

QnResourceAccessSubjectsCache* QnCommonModule::resourceAccessSubjectsCache() const
{
    return m_resourceAccessSubjectCache;
}

QnGlobalPermissionsManager* QnCommonModule::globalPermissionsManager() const
{
    return m_globalPermissionsManager;
}

QnSharedResourcesManager* QnCommonModule::sharedResourcesManager() const
{
    return m_sharedResourceManager;
}

QnUuid QnCommonModule::runningInstanceGUID() const
{
    QnMutexLocker lock(&m_mutex);
    return m_runUuid;
}

void QnCommonModule::updateRunningInstanceGuid()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_runUuid = QnUuid::createUuid();
    }
    emit runningInstanceGUIDChanged();
    resetCachedValue(); //< UpdateModuleInformation
}

QnUuid QnCommonModule::dbId() const
{
    QnMutexLocker lock(&m_mutex);
    return m_dbId;
}

void QnCommonModule::setDbId(const QnUuid& uuid)
{
    QnMutexLocker lock(&m_mutex);
    m_dbId = uuid;
}

QDateTime QnCommonModule::startupTime() const
{
    return m_startupTime;
}

QnCommonMessageProcessor* QnCommonModule::messageProcessor() const
{
    return m_messageProcessor;
}

void QnCommonModule::createMessageProcessorInternal(QnCommonMessageProcessor* messageProcessor)
{
    m_messageProcessor = messageProcessor;
    m_runtimeInfoManager->setMessageProcessor(messageProcessor);
    m_cameraHistory->setMessageProcessor(messageProcessor);
}

void QnCommonModule::deleteMessageProcessor()
{
    if (!m_messageProcessor)
        return;
    m_messageProcessor->init(nullptr); // stop receiving notifications
    m_runtimeInfoManager->setMessageProcessor(nullptr);
    m_cameraHistory->setMessageProcessor(nullptr);
    delete m_messageProcessor;
}

std::shared_ptr<ec2::AbstractECConnection> QnCommonModule::ec2Connection() const
{
    if (m_messageProcessor)
        return m_messageProcessor->connection();

    return nullptr;
}

QnUuid QnCommonModule::videowallGuid() const
{
    return m_videowallGuid;
}

void QnCommonModule::setVideowallGuid(const QnUuid &uuid)
{
    m_videowallGuid = uuid;
}

void QnCommonModule::setResourceDiscoveryManager(QnResourceDiscoveryManager* discoveryManager)
{
    if (m_resourceDiscoveryManager)
        delete m_resourceDiscoveryManager;
    m_resourceDiscoveryManager = discoveryManager;
}
