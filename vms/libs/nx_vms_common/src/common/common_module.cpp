// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_module.h"

#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>

#include <api/common_message_processor.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <common/common_meta_types.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_access/providers/intercom_layout_access_provider.h>
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
#include <network/router.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/utils/installation_info.h>

#include <nx_ec/abstract_ec_connection.h>

#include <nx/analytics/plugin_descriptor_manager.h>
#include <nx/analytics/engine_descriptor_manager.h>
#include <nx/analytics/group_descriptor_manager.h>
#include <nx/analytics/object_type_descriptor_manager.h>
#include <nx/analytics/event_type_descriptor_manager.h>

#include <nx/analytics/taxonomy/state_watcher.h>
#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/network/app_info.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/ec2_router.h>
#include <nx/vms/rules/initializer.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/metrics/metrics_storage.h>
#include <audit/audit_manager.h>
#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/resource_data_pool.h>

#include <core/resource/storage_plugin_factory.h>

#include <nx/utils/timer_manager.h>

#include <nx/branding.h>
#include <nx/build_info.h>

#include <utils/media/ffmpeg_helper.h>

using namespace nx;
using namespace nx::vms::common;

//-------------------------------------------------------------------------------------------------
// QnCommonModule::Private

struct QnCommonModule::Private
{
    AbstractCertificateVerifier* certificateVerifier = nullptr;
};

//-------------------------------------------------------------------------------------------------
// QnCommonModule

QnCommonModule::QnCommonModule(bool clientMode,
    core::access::Mode resourceAccessMode,
    QObject* parent)
    :
    QObject(parent),
    d(new Private),
    m_type(clientMode
        ? nx::vms::api::ModuleInformation::clientId()
        : nx::vms::api::ModuleInformation::mediaServerId())
{
    QnCommonMetaTypes::initialize();

    QnFfmpegHelper::registerLogCallback();
    m_timerManager = std::make_unique<nx::utils::TimerManager>("CommonTimerManager");

    m_storagePluginFactory = new QnStoragePluginFactory(this);

    m_cameraDriverRestrictionList = new CameraDriverRestrictionList(this);
    m_licensePool = new QnLicensePool(this);
    m_cameraUserAttributesPool = new QnCameraUserAttributePool(this);
    m_mediaServerUserAttributesPool = new QnMediaServerUserAttributesPool(this);
    m_resourcePropertyDictionary = new QnResourcePropertyDictionary(this);
    m_resourceStatusDictionary = new QnResourceStatusDictionary(this);
    m_serverAdditionalAddressesDictionary = new QnServerAdditionalAddressesDictionary(this);

    m_resourcePool = new QnResourcePool(this);  /*< Depends on nothing. */
    m_layoutTourManager = new QnLayoutTourManager(this); //< Depends on nothing.
    m_eventRuleManager = new nx::vms::event::RuleManager(this); //< Depends on nothing.
    m_vmsRulesEngine = new nx::vms::rules::Engine(
        std::unique_ptr<nx::vms::rules::Router>(new nx::vms::rules::Ec2Router(this)), this); //< Depends on nothing.
    nx::vms::rules::Initializer initializer(this);
    initializer.registerFields();
    m_metrics = std::make_shared<nx::metrics::Storage>(); //< Depends on nothing.
    m_runtimeInfoManager = new QnRuntimeInfoManager(this); //< Depends on nothing.

    m_moduleDiscoveryManager = new nx::vms::discovery::Manager(clientMode, this);
    m_analyticsPluginDescriptorManager = new nx::analytics::PluginDescriptorManager(this);
    m_analyticsEventTypeDescriptorManager = new nx::analytics::EventTypeDescriptorManager(this);
    m_analyticsEngineDescriptorManager = new nx::analytics::EngineDescriptorManager(this);
    m_analyticsGroupDescriptorManager = new nx::analytics::GroupDescriptorManager(this);
    m_analyticsObjectTypeDescriptorManager = new nx::analytics::ObjectTypeDescriptorManager(this);

    m_descriptorContainer = new nx::analytics::taxonomy::DescriptorContainer(this);
    m_taxonomyStateWatcher = new nx::analytics::taxonomy::StateWatcher(this);

    // TODO: bind m_moduleDiscoveryManager to resPool server changes
    m_router = new QnRouter(this, m_moduleDiscoveryManager);

    m_userRolesManager = new QnUserRolesManager(this); //< Depends on nothing.
    m_sharedResourceManager = new QnSharedResourcesManager(this); //< Depends on resource pool and roles.

    // Depends on resource pool and roles.
    m_globalPermissionsManager = new QnGlobalPermissionsManager(resourceAccessMode, this);

    // Depends on resource pool, roles and global permissions.
    m_resourceAccessSubjectCache = new QnResourceAccessSubjectsCache(this);

    // Depends on resource pool, roles and shared resources.
    m_resourceAccessProvider = new core::access::ResourceAccessProvider(resourceAccessMode, this);

    // Some of base providers depend on QnGlobalPermissionsManager and QnSharedResourcesManager.
    m_resourceAccessProvider->addBaseProvider(
        new core::access::PermissionsResourceAccessProvider(resourceAccessMode, this));
    m_resourceAccessProvider->addBaseProvider(
        new core::access::SharedResourceAccessProvider(resourceAccessMode, this));
    m_resourceAccessProvider->addBaseProvider(
        new core::access::SharedLayoutItemAccessProvider(resourceAccessMode, this));
    m_resourceAccessProvider->addBaseProvider(
        new core::access::VideoWallItemAccessProvider(resourceAccessMode, this));
    m_resourceAccessProvider->addBaseProvider(
        new core::access::IntercomLayoutAccessProvider(resourceAccessMode, this));

    // Depends on access provider.
    m_resourceAccessManager = new QnResourceAccessManager(resourceAccessMode, this);

    m_globalSettings = new QnGlobalSettings(this);
    m_cameraHistory = new QnCameraHistoryPool(this);

    /* Init members. */
    m_runUuid = QnUuid::createUuid();
    m_startupTime = QDateTime::currentDateTime();

    m_resourceDataPool = instance<QnResourceDataPool>();
    m_engineVersion = nx::vms::api::SoftwareVersion(nx::build_info::vmsVersion());
}

void QnCommonModule::setModuleGUID(const QnUuid& guid)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_uuid = guid;
    }
    emit moduleInformationChanged();
}

nx::utils::SoftwareVersion QnCommonModule::engineVersion() const
{
    return m_engineVersion;
}

void QnCommonModule::setEngineVersion(const nx::utils::SoftwareVersion& version)
{
    m_engineVersion = version;
}

QnCommonModule::~QnCommonModule()
{
    resourcePool()->threadPool()->waitForDone();
    m_moduleDiscoveryManager->beforeDestroy();
    m_resourceAccessProvider->clear();
    /* Here all singletons will be destroyed, so we guarantee all socket work will stop. */
    clear();
    setResourceDiscoveryManager(nullptr);
}

void QnCommonModule::bindModuleInformation(const QnMediaServerResourcePtr &server)
{
    /* Can't use resourceChanged signal because it's not emited when we are saving server locally. */
    connect(server.data(), &QnMediaServerResource::nameChanged, this,
        &QnCommonModule::moduleInformationChanged);
    connect(server.data(), &QnMediaServerResource::apiUrlChanged, this,
        &QnCommonModule::moduleInformationChanged);
    connect(server.data(), &QnMediaServerResource::serverFlagsChanged, this,
        &QnCommonModule::moduleInformationChanged);
    connect(server.data(), &QnMediaServerResource::primaryAddressChanged, this,
        &QnCommonModule::moduleInformationChanged);

    connect(m_globalSettings, &QnGlobalSettings::systemNameChanged, this,
        &QnCommonModule::moduleInformationChanged);
    connect(m_globalSettings, &QnGlobalSettings::localSystemIdChanged, this,
        &QnCommonModule::moduleInformationChanged);
    connect(m_globalSettings, &QnGlobalSettings::cloudSettingsChanged, this,
        &QnCommonModule::moduleInformationChanged);
    connect(m_globalSettings, &QnGlobalSettings::initialized, this,
        &QnCommonModule::moduleInformationChanged);
}

void QnCommonModule::setRemoteGUID(const QnUuid &guid)
{
    {
        NX_MUTEX_LOCKER lock( &m_mutex );
        if (m_remoteUuid == guid)
            return;
        m_remoteUuid = guid;
    }
    emit remoteIdChanged(guid);
}

QnUuid QnCommonModule::remoteGUID() const {
    NX_MUTEX_LOCKER lock( &m_mutex );
    return m_remoteUuid;
}

QnMediaServerResourcePtr QnCommonModule::currentServer() const
{
    QnUuid serverId = remoteGUID();
    if (serverId.isNull())
        return QnMediaServerResourcePtr();
    return m_resourcePool->getResourceById(serverId).dynamicCast<QnMediaServerResource>();
}

nx::vms::api::ModuleInformation QnCommonModule::moduleInformation() const
{
    const auto server = m_resourcePool->getResourceById<QnMediaServerResource>(moduleGUID());

    NX_MUTEX_LOCKER lock(&m_mutex);
    nx::vms::api::ModuleInformation moduleInformation;
    moduleInformation.protoVersion = nx::vms::api::protocolVersion();
    moduleInformation.osInfo = nx::utils::OsInfo::current();
    moduleInformation.hwPlatform = nx::vms::utils::installationInfo().hwPlatform;
    moduleInformation.brand = nx::branding::brand();
    moduleInformation.customization = nx::branding::customization();
    moduleInformation.cloudHost = nx::network::SocketGlobals::cloud().cloudHost().c_str();
    moduleInformation.realm = nx::network::AppInfo::realm().c_str();

    moduleInformation.systemName = m_globalSettings->systemName();
    moduleInformation.localSystemId = m_globalSettings->localSystemId();
    moduleInformation.cloudSystemId = m_globalSettings->cloudSystemId();

    moduleInformation.type = m_type;
    moduleInformation.id = m_uuid;
    moduleInformation.runtimeId = m_runUuid;
    moduleInformation.version = m_engineVersion;

    // This code works only on server side.
    NX_ASSERT(!moduleGUID().isNull());
    if (server)
    {
        moduleInformation.port = server->getPort();
        moduleInformation.name = server->getName();
        moduleInformation.serverFlags = server->getServerFlags();
        if (moduleInformation.isNewSystem())
            moduleInformation.serverFlags |= nx::vms::api::SF_NewSystem; //< Legacy API compatibility.
        moduleInformation.sslAllowed = server->isSslAllowed();
    }

    if (auto ec2 = ec2Connection())
        moduleInformation.synchronizedTimeMs = ec2->timeSyncManager()->getSyncTime();

    if (!moduleInformation.cloudSystemId.isEmpty())
    {
        const auto cloudAccountName = m_globalSettings->cloudAccountName();
        if (!cloudAccountName.isEmpty())
            moduleInformation.cloudOwnerId = QnUuid::fromArbitraryData(cloudAccountName);
    }

    return moduleInformation;
}

void QnCommonModule::setSystemIdentityTime(qint64 value, const QnUuid& sender)
{
    NX_INFO(this, "System identity time has changed from %1 to %2", m_systemIdentityTime, value);
    m_systemIdentityTime = value;
    emit systemIdentityTimeChanged(value, sender);
}

qint64 QnCommonModule::systemIdentityTime() const
{
    return m_systemIdentityTime;
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
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_runUuid;
}

void QnCommonModule::updateRunningInstanceGuid()
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_runUuid = QnUuid::createUuid();
    }
    emit runningInstanceGUIDChanged();
    emit moduleInformationChanged();
}

QnUuid QnCommonModule::dbId() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_dbId;
}

void QnCommonModule::setDbId(const QnUuid& uuid)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
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
    m_messageProcessor->initializeContext(this);
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
    m_messageProcessor = nullptr;
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

void QnCommonModule::setStandAloneMode(bool value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_standaloneMode != value)
    {
        m_standaloneMode = value;
        lock.unlock();
        emit standAloneModeChanged(value);
    }
}

nx::metrics::Storage* QnCommonModule::metrics() const
{
    return m_metrics.get();
}

std::weak_ptr<nx::metrics::Storage> QnCommonModule::metricsWeakRef() const
{
    return std::weak_ptr<nx::metrics::Storage>(m_metrics);
}

bool QnCommonModule::isStandAloneMode() const
{
    return m_standaloneMode;
}

void QnCommonModule::setAuditManager(QnAuditManager* auditManager)
{
    m_auditManager = auditManager;
}

QnAuditManager* QnCommonModule::auditManager() const
{
    return m_auditManager;
}

void QnCommonModule::setCertificateVerifier(nx::vms::common::AbstractCertificateVerifier* value)
{
    d->certificateVerifier = value;
}

nx::vms::common::AbstractCertificateVerifier* QnCommonModule::certificateVerifier() const
{
    return d->certificateVerifier;
}

CameraDriverRestrictionList* QnCommonModule::cameraDriverRestrictionList() const
{
    return m_cameraDriverRestrictionList;
}

QnResourceDataPool* QnCommonModule::resourceDataPool() const
{
    return m_resourceDataPool;
}

void QnCommonModule::setMediaStatisticsWindowSize(std::chrono::microseconds value)
{
    m_mediaStatisticsWindowSize = value;
}

std::chrono::microseconds QnCommonModule::mediaStatisticsWindowSize() const
{
    return m_mediaStatisticsWindowSize;
}

void QnCommonModule::setMediaStatisticsMaxDurationInFrames(int value)
{
    m_mediaStatisticsMaxDurationInFrames = value;
}

int QnCommonModule::mediaStatisticsMaxDurationInFrames() const
{
    return m_mediaStatisticsMaxDurationInFrames;
}
