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
    
    void setRemoteGUID(const QnUuid& guid);
    QnUuid remoteGUID() const;

    QUrl moduleUrl() const { return m_url; }
    void setModuleUlr(const QUrl& url) { m_url = url; }

    void setLocalSystemName(const QString& value);
    QString localSystemName() const;

    void setDefaultAdminPassword(const QString& password) { m_defaultAdminPassword = password; }
    QString defaultAdminPassword() const { return m_defaultAdminPassword; }

    void setCloudMode(bool value) { m_cloudMode = value; }
    bool isCloudMode() const { return m_cloudMode; }

    QnSoftwareVersion engineVersion() const;
    void setEngineVersion(const QnSoftwareVersion &version);

    void setModuleInformation(const QnModuleInformation &moduleInformation);
    QnModuleInformation moduleInformation() const;

    bool isTranscodeDisabled() const { return m_transcodingDisabled; }
    void setTranscodeDisabled(bool value) { m_transcodingDisabled = value; }
signals:
    void systemNameChanged(const QString &systemName);
    void remoteIdChanged(const QnUuid &id);

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
};

#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

