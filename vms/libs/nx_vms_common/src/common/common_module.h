// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QUrl>
#include <QtCore/QDateTime>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <nx/core/core_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/utils/value_cache.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/software_version.h>
#include <utils/common/instance_storage.h>

class QnStoragePluginFactory;
class QSettings;
class QnRouter;
class QnResourceDiscoveryManager;
class QnAuditManager;
class CameraDriverRestrictionList;

namespace nx::vms::common {

class AbstractCertificateVerifier;
class SystemContext;

} // namespace nx::vms::common

namespace nx::core::access { class ResourceAccessProvider; }
namespace nx::metrics { struct Storage; }
namespace nx::vms::discovery { class Manager; }
namespace nx::vms::event { class RuleManager; }

namespace nx::utils { class TimerManager; }

/**
 * Storage for the application singleton classes, which maintains correct order of the
 * initialization and destruction and provides access to all these singletons.
 *
 * One instance of Common Module corresponds to one application instance (Desktop or Mobile Client
 * or VMS Server). Functional tests can create several Common Module instances to emulate complex
 * System of several VMS Servers.
 *
 * As a temporary solution, QnCommonModule stores SystemContext for now. This is required to
 * simplify transition process.
 */
class NX_VMS_COMMON_API QnCommonModule:
    public QObject,
    public /*mixin*/ QnInstanceStorage
{
    Q_OBJECT

public:
    /**
     * Common-purpose singleton storage.
     * @param systemContext General System Context. Ownership is managed by the caller.
     * @param clientMode Mode of the Module Discovery work.
     */
    explicit QnCommonModule(
        bool clientMode,
        nx::vms::common::SystemContext* systemContext, //< TODO: #sivanov Remove from here.
        QObject* parent = nullptr);
    virtual ~QnCommonModule();

    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    nx::vms::common::SystemContext* systemContext() const;

    QnStoragePluginFactory* storagePluginFactory() const
    {
        return m_storagePluginFactory;
    }

    QnRouter* router() const;

    nx::vms::discovery::Manager* moduleDiscoveryManager() const;

    void setResourceDiscoveryManager(QnResourceDiscoveryManager* discoveryManager);

    QnResourceDiscoveryManager* resourceDiscoveryManager() const
    {
        return m_resourceDiscoveryManager;
    }

    nx::utils::TimerManager* timerManager() const { return m_timerManager.get(); }

    QnUuid dbId() const;
    void setDbId(const QnUuid& uuid);

    /*
    * This timestamp is using for database backup/restore operation.
    * Server has got systemIdentity time after DB restore operation
    * This time help pushing database from current server to all others
    */
    void setSystemIdentityTime(qint64 value, const QnUuid& sender);
    qint64 systemIdentityTime() const;

    nx::vms::api::ModuleInformation moduleInformation() const;

    bool isTranscodeDisabled() const { return m_transcodingDisabled; }
    void setTranscodeDisabled(bool value) { m_transcodingDisabled = value; }

    void setAllowedPeers(const QSet<QnUuid> &peerList) { m_allowedPeers = peerList; }
    QSet<QnUuid> allowedPeers() const { return m_allowedPeers; }

    QDateTime startupTime() const;

    QnUuid videowallGuid() const;
    void setVideowallGuid(const QnUuid &uuid);

    /**
     * Turn on/off connections to the remove peers.
     * Media server will not receive connections from another peers while it is disabled.
     * Hive mode is enabled by default.
     */
    void setStandAloneMode(bool value);
    bool isStandAloneMode() const;

    nx::utils::SoftwareVersion engineVersion() const;
    void setEngineVersion(const nx::utils::SoftwareVersion& version);

    nx::metrics::Storage* metrics() const;
    std::weak_ptr<nx::metrics::Storage> metricsWeakRef() const;

    void setAuditManager(QnAuditManager* auditManager);
    QnAuditManager* auditManager() const;

    /**
     * Set certificate verifier. Ownership is not controlled. Object must exist for all the class
     * lifetime.
     */
    void setCertificateVerifier(nx::vms::common::AbstractCertificateVerifier* value);

    nx::vms::common::AbstractCertificateVerifier* certificateVerifier() const;

    /** instanceCounter used for unit test purpose only */

    CameraDriverRestrictionList* cameraDriverRestrictionList() const;

    //---------------------------------------------------------------------------------------------
    // Temporary methods for the migration simplification.
    QnUuid peerId() const;
    QnUuid sessionId() const;
    QnLicensePool* licensePool() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;
    QnResourcePool* resourcePool() const;
    QnResourceAccessManager* resourceAccessManager() const;
    nx::core::access::ResourceAccessProvider* resourceAccessProvider() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnGlobalPermissionsManager* globalPermissionsManager() const;
    QnSharedResourcesManager* sharedResourcesManager() const;
    QnUserRolesManager* userRolesManager() const;
    QnCameraHistoryPool* cameraHistoryPool() const;
    QnResourcePropertyDictionary* resourcePropertyDictionary() const;
    QnResourceStatusDictionary* resourceStatusDictionary() const;
    nx::vms::common::SystemSettings* globalSettings() const;
    QnLayoutTourManager* layoutTourManager() const;
    nx::vms::event::RuleManager* eventRuleManager() const;
    QnResourceDataPool* resourceDataPool() const;
    std::shared_ptr<ec2::AbstractECConnection> ec2Connection() const;
    QnCommonMessageProcessor* messageProcessor() const;
    //---------------------------------------------------------------------------------------------

signals:
    void systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void standAloneModeChanged(bool value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
    std::unique_ptr<nx::utils::TimerManager> m_timerManager;
    std::shared_ptr<nx::metrics::Storage> m_metrics;

    const QString m_type;
    QnUuid m_dbId;
    mutable nx::Mutex m_mutex;
    bool m_transcodingDisabled = false;
    QSet<QnUuid> m_allowedPeers;
    qint64 m_systemIdentityTime = 0;

    QDateTime m_startupTime;

    QnStoragePluginFactory* m_storagePluginFactory = nullptr;

    QnResourceDiscoveryManager* m_resourceDiscoveryManager = nullptr;

    QnAuditManager* m_auditManager = nullptr;
    CameraDriverRestrictionList* m_cameraDriverRestrictionList = nullptr;

    QnUuid m_videowallGuid;
    bool m_standaloneMode = false;
    nx::utils::SoftwareVersion m_engineVersion;
};
