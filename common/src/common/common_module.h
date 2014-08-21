#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QUuid>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#include <utils/common/singleton.h>
#include <utils/common/instance_storage.h>
#include <utils/common/software_version.h>
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

    void setModuleGUID(const QUuid& guid) { m_uuid = guid; }
    QUuid moduleGUID() const{ return m_uuid; }

    void setObsoleteServerGuid(const QUuid& guid) { m_obsoleteUuid = guid; }
    QUuid obsoleteServerGuid() const{ return m_obsoleteUuid; }
    
    void setRemoteGUID(const QUuid& guid) {
        QMutexLocker lock(&m_mutex);
        m_remoteUuid = guid; 
    }
    QUuid remoteGUID() const{ 
        QMutexLocker lock(&m_mutex);
        return m_remoteUuid; 
    }

    QUrl moduleUrl() const { return m_url; }
    void setModuleUlr(const QUrl& url) { m_url = url; }

    void setLocalSystemName(const QString& value);
    QString localSystemName() const;
    QByteArray getSystemPassword() const { return "{61D85D22-E7AA-44EC-B5EC-1BEAC9FE19C5}"; }

    void setDefaultAdminPassword(const QString& password) { m_defaultAdminPassword = password; }
    QString defaultAdminPassword() const { return m_defaultAdminPassword; }

    void setCloudMode(bool value) { m_cloudMode = value; }
    bool isCloudMode() const { return m_cloudMode; }

    QnSoftwareVersion engineVersion() const;
    void setEngineVersion(const QnSoftwareVersion &version);

    void setModuleInformation( const QnModuleInformation& moduleInformation );
    QnModuleInformation moduleInformation() const;

signals:
    void systemNameChanged(const QString &systemName);

protected:
    static void loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required);

private:
    QnSessionManager *m_sessionManager;
    QnResourceDataPool *m_dataPool;
    QString m_localSystemName;
    QString m_defaultAdminPassword;
    QUuid m_uuid;
    QUuid m_obsoleteUuid;
    QUuid m_remoteUuid;
    QUrl m_url;
    bool m_cloudMode;
    QnSoftwareVersion m_engineVersion;
    QnModuleInformation m_moduleInformation;
    mutable QMutex m_mutex;
};

#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

