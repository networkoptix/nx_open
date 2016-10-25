#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QtCore/QObject>
#include <QtCore/QUrl>

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
struct AdminPasswordData
{
    void saveToSettings(QSettings* settings);
    void loadFromSettings(const QSettings* settings);
    bool isEmpty() const;

    static void clearSettings(QSettings* settings);

    QByteArray digest;
    QByteArray hash;
    QByteArray cryptSha512Hash;
    QByteArray realm;
};


class QnResourceDataPool;

/**
 * Storage for common module's global state.
 *
 * All singletons and initialization/deinitialization code goes here.
 */
class QnCommonModule: public QObject, public QnInstanceStorage, public Singleton<QnCommonModule>
{
    Q_OBJECT
public:
    QnCommonModule(QObject *parent = NULL);
    virtual ~QnCommonModule();

    using Singleton<QnCommonModule>::instance;
    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    void bindModuleinformation(const QnMediaServerResourcePtr &server);

    QnResourceDataPool *dataPool() const {
        return m_dataPool;
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
    void setUseLowPriorityAdminPasswordHach(bool value);
    bool useLowPriorityAdminPasswordHach() const;

    void setAdminPasswordData(const AdminPasswordData& data);
    AdminPasswordData adminPasswordData() const;

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

signals:
    void readOnlyChanged(bool readOnly);
    void moduleInformationChanged();
    void remoteIdChanged(const QnUuid &id);
    void systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void runningInstanceGUIDChanged();
protected:
    static void loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required);
private:
    void resetCachedValue();
    void updateModuleInformationUnsafe();
private:
    bool m_dirtyModuleInformation;
    QnResourceDataPool *m_dataPool;
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

    AdminPasswordData m_adminPasswordData;
    bool m_lowPriorityAdminPassword;
    Qn::PeerType m_localPeerType;
};

#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

