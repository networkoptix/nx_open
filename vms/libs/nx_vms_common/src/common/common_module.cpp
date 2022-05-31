// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_module.h"

#include <cassert>

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>

#include <audit/audit_manager.h>
#include <common/common_meta_types.h>
#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <network/router.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <utils/media/ffmpeg_helper.h>

using namespace nx;
using namespace nx::vms::common;

//-------------------------------------------------------------------------------------------------
// QnCommonModule::Private

struct QnCommonModule::Private
{
    QPointer<nx::vms::common::SystemContext> systemContext;
    AbstractCertificateVerifier* certificateVerifier = nullptr;
    QPointer<nx::vms::discovery::Manager> moduleDiscoveryManager = nullptr;
    std::unique_ptr<QnRouter> router;
};

//-------------------------------------------------------------------------------------------------
// QnCommonModule

QnCommonModule::QnCommonModule(
    nx::vms::common::SystemContext* systemContext,
    nx::vms::discovery::Manager* moduleDiscoveryManager,
    QObject* parent)
    :
    QObject(parent),
    d(new Private)
{
    d->systemContext = systemContext;
    d->moduleDiscoveryManager = moduleDiscoveryManager;
    QnCommonMetaTypes::initialize();

    QnFfmpegHelper::registerLogCallback();
    m_timerManager = std::make_unique<nx::utils::TimerManager>("CommonTimerManager");

    m_cameraDriverRestrictionList = new CameraDriverRestrictionList(this);

    m_metrics = std::make_shared<nx::metrics::Storage>(); //< Depends on nothing.

    if (moduleDiscoveryManager) //< Can be absent in unit tests.
    {
        d->router = std::make_unique<QnRouter>(d->moduleDiscoveryManager);
        systemContext->initNetworking(d->router.get(), d->certificateVerifier);
    }

    /* Init members. */
    m_startupTime = QDateTime::currentDateTime();

    m_engineVersion = nx::vms::api::SoftwareVersion(nx::build_info::vmsVersion());
}

nx::vms::common::SystemContext* QnCommonModule::systemContext() const
{
    return d->systemContext.data();
}

QnRouter* QnCommonModule::router() const
{
    return d->router.get();
}

nx::vms::discovery::Manager* QnCommonModule::moduleDiscoveryManager() const
{
    return d->moduleDiscoveryManager;
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
    /* Here all singletons will be destroyed, so we guarantee all socket work will stop. */
    clear();
    setResourceDiscoveryManager(nullptr);
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
    d->systemContext->initNetworking(d->router.get(), d->certificateVerifier);
}

nx::vms::common::AbstractCertificateVerifier* QnCommonModule::certificateVerifier() const
{
    return d->certificateVerifier;
}

CameraDriverRestrictionList* QnCommonModule::cameraDriverRestrictionList() const
{
    return m_cameraDriverRestrictionList;
}

//-------------------------------------------------------------------------------------------------
// Temporary methods for the migration simplification.

QnUuid QnCommonModule::peerId() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->peerId()
        : QnUuid();
}

QnUuid QnCommonModule::sessionId() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->sessionId()
        : QnUuid();
}

QnLicensePool* QnCommonModule::licensePool() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->licensePool()
        : nullptr;
}

QnRuntimeInfoManager* QnCommonModule::runtimeInfoManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->runtimeInfoManager()
        : nullptr;
}

QnResourcePool* QnCommonModule::resourcePool() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourcePool()
        : nullptr;
}

QnResourceAccessManager* QnCommonModule::resourceAccessManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceAccessManager()
        : nullptr;
}

nx::core::access::ResourceAccessProvider* QnCommonModule::resourceAccessProvider() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceAccessProvider()
        : nullptr;
}

QnResourceAccessSubjectsCache* QnCommonModule::resourceAccessSubjectsCache() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceAccessSubjectsCache()
        : nullptr;
}

QnGlobalPermissionsManager* QnCommonModule::globalPermissionsManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->globalPermissionsManager()
        : nullptr;
}

QnSharedResourcesManager* QnCommonModule::sharedResourcesManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->sharedResourcesManager()
        : nullptr;
}

QnUserRolesManager* QnCommonModule::userRolesManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->userRolesManager()
        : nullptr;
}

QnCameraHistoryPool* QnCommonModule::cameraHistoryPool() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->cameraHistoryPool()
        : nullptr;
}

QnResourcePropertyDictionary* QnCommonModule::resourcePropertyDictionary() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourcePropertyDictionary()
        : nullptr;
}

QnResourceStatusDictionary* QnCommonModule::resourceStatusDictionary() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceStatusDictionary()
        : nullptr;
}

SystemSettings* QnCommonModule::globalSettings() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->globalSettings()
        : nullptr;
}

QnLayoutTourManager* QnCommonModule::layoutTourManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->layoutTourManager()
        : nullptr;
}

nx::vms::event::RuleManager* QnCommonModule::eventRuleManager() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->eventRuleManager()
        : nullptr;
}

QnResourceDataPool* QnCommonModule::resourceDataPool() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->resourceDataPool()
        : nullptr;
}

std::shared_ptr<ec2::AbstractECConnection> QnCommonModule::ec2Connection() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->ec2Connection()
        : nullptr;
}

QnCommonMessageProcessor* QnCommonModule::messageProcessor() const
{
    return NX_ASSERT(d->systemContext)
        ? d->systemContext->messageProcessor()
        : nullptr;
}

//-------------------------------------------------------------------------------------------------
