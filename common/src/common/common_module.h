#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#include <utils/common/singleton.h>
#include <utils/common/instance_storage.h>
#include <utils/common/software_version.h>
#include <utils/common/uuid.h>
#include <utils/network/module_information.h>
#include "nx_ec/data/api_runtime_data.h"

class QnSessionManager;
class QnResourceDataPool;

/**
 * Storage for common module's global state.
 * 
 * All singletons and initialization/deinitialization code goes here.
 */
class QnCommonModule: public QObject, public QnInstanceStorage, public Singleton<QnCommonModule> {
    Q_OBJECT
public:
    QnCommonModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnCommonModule();

    using Singleton<QnCommonModule>::instance;
    using QnInstanceStorage::instance;

    QnResourceDataPool *dataPool() const {
        return m_dataPool;
    }

    QnSessionManager *sessionManager() const {
        return m_sessionManager;
    }

    void setModuleGUID(const QnUuid& guid) { m_uuid = guid; }
    QnUuid moduleGUID() const{ return m_uuid; }

    QnUuid runningInstanceGUID() const{ return m_runUuid; }

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

    QUrl moduleUrl() const { return m_url; }
    void setModuleUlr(const QUrl& url) { m_url = url; }

    void setLocalSystemName(const QString& value);
    QString localSystemName() const;

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

    void setAdminPasswordData(const QByteArray& hash, const QByteArray& digest);
    void adminPasswordData(QByteArray* hash, QByteArray* digest) const;

    void setCloudMode(bool value) { m_cloudMode = value; }
    bool isCloudMode() const { return m_cloudMode; }

    QnSoftwareVersion engineVersion() const;
    void setEngineVersion(const QnSoftwareVersion &version);

    void setModuleInformation(const QnModuleInformation &moduleInformation);
    QnModuleInformation moduleInformation() const;

    bool isTranscodeDisabled() const { return m_transcodingDisabled; }
    void setTranscodeDisabled(bool value) { m_transcodingDisabled = value; }

    inline void setAllowedPeers(const QSet<QnUuid> &peerList) { m_allowedPeers = peerList; }
    inline QSet<QnUuid> allowedPeers() const { return m_allowedPeers; }
signals:
    void systemNameChanged(const QString &systemName);
    void remoteIdChanged(const QnUuid &id);
    void systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
protected:
    static void loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required);

private:
    QnSessionManager *m_sessionManager;
    QnResourceDataPool *m_dataPool;
    QString m_localSystemName;
    QString m_defaultAdminPassword;
    QnUuid m_uuid;
    QnUuid m_runUuid;
    QnUuid m_obsoleteUuid;
    QnUuid m_remoteUuid;
    QUrl m_url;
    bool m_cloudMode;
    QnSoftwareVersion m_engineVersion;
    QnModuleInformation m_moduleInformation;
    mutable QMutex m_mutex;
    bool m_transcodingDisabled;
    QSet<QnUuid> m_allowedPeers;
    qint64 m_systemIdentityTime;
    
    QByteArray m_adminPaswdHash;
    QByteArray m_adminPaswdDigest;
    bool m_lowPriorityAdminPassword;
};

#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

