#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QDateTime>

#include <common/common_module_aware.h>

#include <nx/core/core_fwd.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/singleton.h>
#include <utils/common/instance_storage.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <network/module_information.h>
#include <nx_ec/data/api_runtime_data.h>
#include <utils/common/value_cache.h>
#include <plugins/native_sdk/common_plugin_container.h>

class QSettings;
class QnSessionManager;
class QnRouter;
class QnGlobalSettings;
class QnCommonMessageProcessor;
class QnResourceAccessManager;
class QnResourceAccessProvider;
class QnUserRolesManager;
class QnSharedResourcesManager;
class QnCameraUserAttributePool;
class QnMediaServerUserAttributesPool;
class QnResourcePropertyDictionary;
class QnResourceStatusDictionary;
class QnResourceDiscoveryManager;
class QnServerAdditionalAddressesDictionary;

namespace nx { namespace vms { namespace event { class RuleManager; }}}

namespace ec2 { class AbstractECConnection; }
namespace nx { namespace vms { namespace discovery { class Manager; }}}

struct BeforeRestoreDbData
{
    void saveToSettings(QSettings* settings);
    void loadFromSettings(const QSettings* settings);
    bool isEmpty() const;

    bool hasInfoForStorage(const QString& url) const;
    qint64 getSpaceLimitForStorage(const QString& url) const;

    static void clearSettings(QSettings* settings);

    QByteArray digest;
    QByteArray hash;
    QByteArray cryptSha512Hash;
    QByteArray realm;
    QByteArray localSystemId;
    QByteArray localSystemName;
    QByteArray serverName;
    QByteArray storageInfo;
};


class QnResourceDataPool;

/**
 * Storage for common module's global state.
 *
 * All singletons and initialization/deinitialization code goes here.
 */
class QnCommonModule: public QObject, public QnInstanceStorage
{
    Q_OBJECT
public:
    explicit QnCommonModule(bool clientMode,
        nx::core::access::Mode resourceAccessMode,
        QObject* parent = nullptr);
    virtual ~QnCommonModule();

    //using Singleton<QnCommonModule>::instance;
    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    void bindModuleInformation(const QnMediaServerResourcePtr &server);

    QnSessionManager* sessionManager() const
    {
        return m_sessionManager;
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

    QnResourceAccessProvider* resourceAccessProvider() const
    {
        return m_resourceAccessProvider;
    }

    QnResourcePropertyDictionary* propertyDictionary() const
    {
        return m_resourcePropertyDictionary;
    }

    QnResourceStatusDictionary* statusDictionary() const
    {
        return m_resourceStatusDictionary;
    }

    QnServerAdditionalAddressesDictionary* serverAdditionalAddressesDictionary() const
    {
        return m_serverAdditionalAddressesDictionary;
    }

    QnCameraUserAttributePool* cameraUserAttributesPool() const
    {
        return m_cameraUserAttributesPool;
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

    QnLicensePool* licensePool() const;
    QnUserRolesManager* userRolesManager() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnGlobalPermissionsManager* globalPermissionsManager() const;
    QnSharedResourcesManager* sharedResourcesManager() const;

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

    void setRemoteGUID(const QnUuid& guid);
    QnUuid remoteGUID() const;

    /** Url we are currently connected to. */
    QUrl currentUrl() const;

    /** Server we are currently connected to. */
    QnMediaServerResourcePtr currentServer() const;

    void setReadOnly(bool value);
    bool isReadOnly() const;

    void setDefaultAdminPassword(const QString& password) { m_defaultAdminPassword = password; }
    QString defaultAdminPassword() const { return m_defaultAdminPassword; }

    /*
    * This function should resolve issue with install new media servers and connect their to current system
    * Installer tell media server change password after insllation.
    * At this case admin user will rewritted. To keep other admin user field unchanged (email settings)
    * we have to insert new transaction with low priority
    */
    void setUseLowPriorityAdminPasswordHack(bool value);
    bool useLowPriorityAdminPasswordHack() const;

    void setBeforeRestoreData(const BeforeRestoreDbData& data);
    BeforeRestoreDbData beforeRestoreDbData() const;

    void setCloudMode(bool value) { m_cloudMode = value; }
    bool isCloudMode() const { return m_cloudMode; }

    void setModuleInformation(const QnModuleInformation& moduleInformation);
    QnModuleInformation moduleInformation();

    bool isTranscodeDisabled() const { return m_transcodingDisabled; }
    void setTranscodeDisabled(bool value) { m_transcodingDisabled = value; }

    inline void setAllowedPeers(const QSet<QnUuid> &peerList) { m_allowedPeers = peerList; }
    inline QSet<QnUuid> allowedPeers() const { return m_allowedPeers; }

    QDateTime startupTime() const;

    QnCommonMessageProcessor* messageProcessor() const;

    template <class MessageProcessorType> void createMessageProcessor()
    {
        createMessageProcessorInternal(new MessageProcessorType(this));
    }
    void deleteMessageProcessor();

    std::shared_ptr<ec2::AbstractECConnection> ec2Connection() const;

    QnUuid videowallGuid() const;
    void setVideowallGuid(const QnUuid &uuid);

    /** instanceCounter used for unit test purpose only */
signals:
    void readOnlyChanged(bool readOnly);
    void moduleInformationChanged();
    void remoteIdChanged(const QnUuid &id);
    void systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void runningInstanceGUIDChanged();
private:
    void createMessageProcessorInternal(QnCommonMessageProcessor* messageProcessor);
    void resetCachedValue();
    void updateModuleInformationUnsafe();

private:
    bool m_dirtyModuleInformation;
    QnSessionManager* m_sessionManager = nullptr;
    QnResourcePool* m_resourcePool = nullptr;
    QnResourceAccessSubjectsCache* m_resourceAccessSubjectCache = nullptr;
    QnSharedResourcesManager* m_sharedResourceManager = nullptr;
    nx::vms::discovery::Manager* m_moduleDiscoveryManager = nullptr;
    QnRouter* m_router = nullptr;

    QString m_defaultAdminPassword;
    QnUuid m_uuid;
    QnUuid m_runUuid;
    QnUuid m_dbId;
    QnUuid m_obsoleteUuid;
    QnUuid m_remoteUuid;
    bool m_cloudMode;
    QnSoftwareVersion m_engineVersion;
    QnModuleInformation m_moduleInformation;
    mutable QnMutex m_mutex;
    bool m_transcodingDisabled = false;
    QSet<QnUuid> m_allowedPeers;
    qint64 m_systemIdentityTime = 0;

    BeforeRestoreDbData m_beforeRestoreDbData;
    bool m_lowPriorityAdminPassword = false;
    QDateTime m_startupTime;

    QnGlobalSettings* m_globalSettings = nullptr;
    QnCameraHistoryPool* m_cameraHistory = nullptr;
    QnCommonMessageProcessor* m_messageProcessor = nullptr;
    QnRuntimeInfoManager* m_runtimeInfoManager = nullptr;
    QnResourceAccessManager* m_resourceAccessManager = nullptr;
    QnResourceAccessProvider* m_resourceAccessProvider = nullptr;
    QnLicensePool* m_licensePool = nullptr;
    QnCameraUserAttributePool* m_cameraUserAttributesPool = nullptr;
    QnMediaServerUserAttributesPool* m_mediaServerUserAttributesPool = nullptr;
    QnResourcePropertyDictionary* m_resourcePropertyDictionary = nullptr;
    QnResourceStatusDictionary* m_resourceStatusDictionary = nullptr;
    QnServerAdditionalAddressesDictionary* m_serverAdditionalAddressesDictionary = nullptr;
    QnGlobalPermissionsManager* m_globalPermissionsManager = nullptr;
    QnUserRolesManager* m_userRolesManager = nullptr;
    QnResourceDiscoveryManager* m_resourceDiscoveryManager = nullptr;
    QnLayoutTourManager* m_layoutTourManager = nullptr;
    nx::vms::event::RuleManager* m_eventRuleManager = nullptr;

    // TODO: #dmishin move these factories to server module
    QnUuid m_videowallGuid;
};
