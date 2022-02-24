// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QUrl>
#include <QtCore/QDateTime>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/instance_storage.h>
#include <nx/utils/value_cache.h>

#include <nx/core/core_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/api/data/module_information.h>

class QnStoragePluginFactory;
class QSettings;
class QnRouter;
class QnGlobalSettings;
class QnCommonMessageProcessor;
class QnResourceAccessManager;
class QnUserRolesManager;
class QnSharedResourcesManager;
class QnMediaServerUserAttributesPool;
class QnResourcePropertyDictionary;
class QnResourceStatusDictionary;
class QnResourceDiscoveryManager;
class QnServerAdditionalAddressesDictionary;
class QnAuditManager;
class CameraDriverRestrictionList;
class QnResourceDataPool;

namespace ec2 { class AbstractECConnection; }

namespace nx::analytics {

class PluginDescriptorManager;
class EventTypeDescriptorManager;
class EngineDescriptorManager;
class GroupDescriptorManager;
class ObjectTypeDescriptorManager;

namespace taxonomy {

class AbstractStateWatcher;
class DescriptorContainer;

} // namespace taxonomy

} // namespace nx::analytics;

namespace nx::core::access { class ResourceAccessProvider; }
namespace nx::metrics { struct Storage; }
namespace nx::vms::common { class AbstractCertificateVerifier; }
namespace nx::vms::discovery { class Manager; }
namespace nx::vms::event { class RuleManager; }
namespace nx::vms::rules { class Engine; }

namespace nx::utils { class TimerManager; }

class QnResourceDataPool;

/**
 * Storage for common module's global state.
 *
 * All singletons and initialization/deinitialization code goes here.
 */
class NX_VMS_COMMON_API QnCommonModule:
    public QObject,
    public /*mixin*/ QnInstanceStorage
{
    Q_OBJECT
public:
    explicit QnCommonModule(bool clientMode,
        nx::core::access::Mode resourceAccessMode,
        QObject* parent = nullptr);
    virtual ~QnCommonModule();

    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    void bindModuleInformation(const QnMediaServerResourcePtr &server);

    QnStoragePluginFactory* storagePluginFactory() const
    {
        return m_storagePluginFactory;
    }

    QnResourcePool* resourcePool() const
    {
        return m_resourcePool;
    }

    QnRouter* router() const
    {
        return m_router;
    }

    QnGlobalSettings* globalSettings() const
    {
        return m_globalSettings;
    }

    nx::vms::discovery::Manager* moduleDiscoveryManager() const
    {
        return m_moduleDiscoveryManager;
    }

    QnCameraHistoryPool* cameraHistoryPool() const
    {
        return m_cameraHistory;
    }

    QnRuntimeInfoManager* runtimeInfoManager() const
    {
        return m_runtimeInfoManager;
    }

    QnResourceAccessManager* resourceAccessManager() const
    {
        return m_resourceAccessManager;
    }

    nx::core::access::ResourceAccessProvider* resourceAccessProvider() const
    {
        return m_resourceAccessProvider;
    }

    QnResourcePropertyDictionary* resourcePropertyDictionary() const
    {
        return m_resourcePropertyDictionary;
    }

    QnResourceStatusDictionary* resourceStatusDictionary() const
    {
        return m_resourceStatusDictionary;
    }

    QnServerAdditionalAddressesDictionary* serverAdditionalAddressesDictionary() const
    {
        return m_serverAdditionalAddressesDictionary;
    }

    QnMediaServerUserAttributesPool* mediaServerUserAttributesPool() const
    {
        return m_mediaServerUserAttributesPool;
    }

    void setResourceDiscoveryManager(QnResourceDiscoveryManager* discoveryManager);

    QnResourceDiscoveryManager* resourceDiscoveryManager() const
    {
        return m_resourceDiscoveryManager;
    }

    QnLayoutTourManager* layoutTourManager() const
    {
        return m_layoutTourManager;
    }

    nx::vms::event::RuleManager* eventRuleManager() const
    {
        return m_eventRuleManager;
    }

    nx::vms::rules::Engine* vmsRulesEngine() const
    {
        return m_vmsRulesEngine;
    }

    nx::analytics::PluginDescriptorManager* analyticsPluginDescriptorManager() const
    {
        return m_analyticsPluginDescriptorManager;
    }

    nx::analytics::EventTypeDescriptorManager* analyticsEventTypeDescriptorManager() const
    {
        return m_analyticsEventTypeDescriptorManager;
    }

    nx::analytics::EngineDescriptorManager* analyticsEngineDescriptorManager() const
    {
        return m_analyticsEngineDescriptorManager;
    }

    nx::analytics::GroupDescriptorManager* analyticsGroupDescriptorManager() const
    {
        return m_analyticsGroupDescriptorManager;
    }

    nx::analytics::ObjectTypeDescriptorManager* analyticsObjectTypeDescriptorManager() const
    {
        return m_analyticsObjectTypeDescriptorManager;
    }

    nx::analytics::taxonomy::DescriptorContainer* descriptorContainer() const
    {
        return m_descriptorContainer;
    }

    nx::analytics::taxonomy::AbstractStateWatcher* taxonomyStateWatcher() const
    {
        return m_taxonomyStateWatcher;
    }

    nx::utils::TimerManager* timerManager() const { return m_timerManager.get(); }

    void setNeedToStop(bool value) { m_needToStop = value; }
    bool isNeedToStop() const { return m_needToStop; }

    QnLicensePool* licensePool() const;
    QnUserRolesManager* userRolesManager() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnGlobalPermissionsManager* globalPermissionsManager() const;
    QnSharedResourcesManager* sharedResourcesManager() const;

    /** m_uuid is the server identifier for a p2p connection */
    void setModuleGUID(const QnUuid& guid);
    QnUuid moduleGUID() const{ return m_uuid; }

    QnUuid runningInstanceGUID() const;
    void updateRunningInstanceGuid();

    void setObsoleteServerGuid(const QnUuid& guid) { m_obsoleteUuid = guid; }
    QnUuid obsoleteServerGuid() const{ return m_obsoleteUuid; }

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

    QnCommonMessageProcessor* messageProcessor() const;

    template <class MessageProcessorType, typename ...Args>
    MessageProcessorType* createMessageProcessor(Args... args)
    {
        const auto processor = new MessageProcessorType(args...);
        createMessageProcessorInternal(processor);
        return processor;
    }

    void deleteMessageProcessor();

    std::shared_ptr<ec2::AbstractECConnection> ec2Connection() const;

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

    QnResourceDataPool* resourceDataPool() const;

    void setMediaStatisticsWindowSize(std::chrono::microseconds value);
    std::chrono::microseconds mediaStatisticsWindowSize() const;

    void setMediaStatisticsMaxDurationInFrames(int value);
    int mediaStatisticsMaxDurationInFrames() const;

signals:
    void moduleInformationChanged();
    void systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void runningInstanceGUIDChanged();
    void standAloneModeChanged(bool value);

private:
    void createMessageProcessorInternal(QnCommonMessageProcessor* messageProcessor);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
    std::unique_ptr<nx::utils::TimerManager> m_timerManager;
    std::shared_ptr<nx::metrics::Storage> m_metrics;
    QnResourcePool* m_resourcePool = nullptr;
    QnResourceAccessSubjectsCache* m_resourceAccessSubjectCache = nullptr;
    QnSharedResourcesManager* m_sharedResourceManager = nullptr;
    nx::vms::discovery::Manager* m_moduleDiscoveryManager = nullptr;
    QnRouter* m_router = nullptr;

    const QString m_type;
    QnUuid m_uuid;
    QnUuid m_runUuid;
    QnUuid m_dbId;
    QnUuid m_obsoleteUuid;
    mutable nx::Mutex m_mutex;
    bool m_transcodingDisabled = false;
    QSet<QnUuid> m_allowedPeers;
    qint64 m_systemIdentityTime = 0;

    QDateTime m_startupTime;

    QnStoragePluginFactory* m_storagePluginFactory = nullptr;
    QnGlobalSettings* m_globalSettings = nullptr;
    QnCameraHistoryPool* m_cameraHistory = nullptr;
    QnCommonMessageProcessor* m_messageProcessor = nullptr;
    QnRuntimeInfoManager* m_runtimeInfoManager = nullptr;
    QnResourceAccessManager* m_resourceAccessManager = nullptr;
    nx::core::access::ResourceAccessProvider* m_resourceAccessProvider = nullptr;
    QnLicensePool* m_licensePool = nullptr;
    QnMediaServerUserAttributesPool* m_mediaServerUserAttributesPool = nullptr;
    QnResourcePropertyDictionary* m_resourcePropertyDictionary = nullptr;
    QnResourceStatusDictionary* m_resourceStatusDictionary = nullptr;
    QnServerAdditionalAddressesDictionary* m_serverAdditionalAddressesDictionary = nullptr;
    QnGlobalPermissionsManager* m_globalPermissionsManager = nullptr;
    QnUserRolesManager* m_userRolesManager = nullptr;
    QnResourceDiscoveryManager* m_resourceDiscoveryManager = nullptr;
    QnLayoutTourManager* m_layoutTourManager = nullptr;
    nx::vms::event::RuleManager* m_eventRuleManager = nullptr;
    nx::vms::rules::Engine* m_vmsRulesEngine = nullptr;
    QnAuditManager* m_auditManager = nullptr;
    CameraDriverRestrictionList* m_cameraDriverRestrictionList = nullptr;
    QnResourceDataPool* m_resourceDataPool = nullptr;

    nx::analytics::PluginDescriptorManager* m_analyticsPluginDescriptorManager = nullptr;
    nx::analytics::EventTypeDescriptorManager* m_analyticsEventTypeDescriptorManager = nullptr;
    nx::analytics::EngineDescriptorManager* m_analyticsEngineDescriptorManager = nullptr;
    nx::analytics::GroupDescriptorManager* m_analyticsGroupDescriptorManager = nullptr;
    nx::analytics::ObjectTypeDescriptorManager* m_analyticsObjectTypeDescriptorManager = nullptr;

    nx::analytics::taxonomy::DescriptorContainer* m_descriptorContainer = nullptr;
    nx::analytics::taxonomy::AbstractStateWatcher* m_taxonomyStateWatcher = nullptr;

    QnUuid m_videowallGuid;
    bool m_standaloneMode = false;
    std::atomic<bool> m_needToStop{false};
    nx::utils::SoftwareVersion m_engineVersion;

    std::chrono::microseconds m_mediaStatisticsWindowSize = std::chrono::seconds(2);
    int m_mediaStatisticsMaxDurationInFrames = 0;
};
