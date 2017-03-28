#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QDateTime>

#include <core/resource/resource_fwd.h>

#include <nx/utils/singleton.h>
#include <utils/common/instance_storage.h>
#include <utils/common/software_version.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include "network/module_information.h"
#include "nx_ec/data/api_runtime_data.h"
#include <utils/common/value_cache.h>

class QSettings;
class QnSessionManager;
class QnModuleFinder;
class QnRouter;
class QnGlobalSettings;
class QnCameraHistoryPool;
class QnGlobalPermissionsManager;
class QnRuntimeInfoManager;
class QnCommonMessageProcessor;
class QnResourceAccessManager;
class QnResourceAccessProvider;

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
    QnCommonModule(bool clientMode, QObject *parent = nullptr);
    virtual ~QnCommonModule();

    //using Singleton<QnCommonModule>::instance;
    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    void bindModuleinformation(const QnMediaServerResourcePtr &server);

    QnResourceDataPool *dataPool() const
    {
        return m_dataPool;
    }

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

    QnModuleFinder* moduleFinder() const
    {
        return m_moduleFinder;
    }

    QnCameraHistoryPool* cameraHistoryPool() const
    {
        return m_cameraHistory;
    }

    QnGlobalPermissionsManager* globalPermissionsManager() const
    {
        return m_globalPermissionManager;
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

    void setModuleGUID(const QnUuid& guid) { m_uuid = guid; }
    QnUuid moduleGUID() const{ return m_uuid; }

    QnUuid runningInstanceGUID() const;
    void updateRunningInstanceGuid();

    void setObsoleteServerGuid(const QnUuid& guid) { m_obsoleteUuid = guid; }
    QnUuid obsoleteServerGuid() const{ return m_obsoleteUuid; }

    /*
    * This timestamp is using for database backup/restore operation.
    * Server has got systemIdentity time after DB restore operation
    * This time help pushing database from current server to all others
    */
    void setSystemIdentityTime(qint64 value, const QnUuid& sender);
    qint64 systemIdentityTime() const;

    void setRemoteGUID(const QnUuid& guid);
    QnUuid remoteGUID() const;

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

    QnSoftwareVersion engineVersion() const;
    void setEngineVersion(const QnSoftwareVersion &version);

    void setModuleInformation(const QnModuleInformation& moduleInformation);
    QnModuleInformation moduleInformation();

    bool isTranscodeDisabled() const { return m_transcodingDisabled; }
    void setTranscodeDisabled(bool value) { m_transcodingDisabled = value; }

    inline void setAllowedPeers(const QSet<QnUuid> &peerList) { m_allowedPeers = peerList; }
    inline QSet<QnUuid> allowedPeers() const { return m_allowedPeers; }

    void setLocalPeerType(Qn::PeerType peerType);
    Qn::PeerType localPeerType() const;
    QDateTime startupTime() const;

    QnCommonMessageProcessor* messageProcessor() const;
    void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);

signals:
    void readOnlyChanged(bool readOnly);
    void moduleInformationChanged();
    void remoteIdChanged(const QnUuid &id);
    void systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void runningInstanceGUIDChanged();
private:
    void resetCachedValue();
    void updateModuleInformationUnsafe();
private:
    bool m_dirtyModuleInformation;
    QnResourceDataPool *m_dataPool;
    QnSessionManager* m_sessionManager;
    QnResourcePool* m_resourcePool;
    QnModuleFinder* m_moduleFinder;
    QnRouter* m_router;

    QString m_defaultAdminPassword;
    QnUuid m_uuid;
    QnUuid m_runUuid;
    QnUuid m_obsoleteUuid;
    QnUuid m_remoteUuid;
    bool m_cloudMode;
    QnSoftwareVersion m_engineVersion;
    QnModuleInformation m_moduleInformation;
    mutable QnMutex m_mutex;
    bool m_transcodingDisabled;
    QSet<QnUuid> m_allowedPeers;
    qint64 m_systemIdentityTime;

    BeforeRestoreDbData m_beforeRestoreDbData;
    bool m_lowPriorityAdminPassword;
    Qn::PeerType m_localPeerType;
    QDateTime m_startupTime;

    QnGlobalSettings* m_globalSettings;
    QnCameraHistoryPool* m_cameraHistory;
    QnGlobalPermissionsManager* m_globalPermissionManager;
    QnCommonMessageProcessor* m_messageProcessor;
    QnRuntimeInfoManager* m_runtimeInfoManager;
    QnResourceAccessManager* m_resourceAccessManager;
    QnResourceAccessProvider* m_resourceAccessProvider;
};

//#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

