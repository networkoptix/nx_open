#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QUuid>
#include <QtCore/QObject>

#include <utils/common/singleton.h>
#include <utils/common/instance_storage.h>
#include <utils/common/software_version.h>

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
    QUrl moduleUrl() const { return m_url; }
    void setModuleUlr(const QUrl& url) { m_url = url; }

    void setLocalSystemName(const QString& value) { m_localSystemName = value; }
    QString localSystemName() { return m_localSystemName; }
    QByteArray getSystemPassword() { return "{61D85D22-E7AA-44EC-B5EC-1BEAC9FE19C5}"; }
    void setCloudMode(bool value) { m_cloudMode = value; }
    bool isCloudMode() const { return m_cloudMode; }

    QnSoftwareVersion engineVersion() const;
    void setEngineVersion(const QnSoftwareVersion &version);

protected:
    static void loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required);

private:
    QnSessionManager *m_sessionManager;
    QnResourceDataPool *m_dataPool;
    QString m_localSystemName;
    QUuid m_uuid;
    QUrl m_url;
    bool m_cloudMode;
    QnSoftwareVersion m_engineVersion;
};

#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

