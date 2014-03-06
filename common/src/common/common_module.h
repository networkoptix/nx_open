#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QUuid>
#include <QtCore/QObject>

#include <utils/common/singleton.h>
#include <utils/common/instance_storage.h>

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

    void setSystemName(const QString& value) { m_systemName = value; }
    QString systemName() { return m_systemName; }
    QByteArray getSystemPassword() { return "{61D85D22-E7AA-44EC-B5EC-1BEAC9FE19C5}"; }

protected:
    static void loadResourceData(QnResourceDataPool *dataPool, const QString &fileName);

private:
    QnSessionManager *m_sessionManager;
    QnResourceDataPool *m_dataPool;
    QString m_systemName;
    QUuid m_uuid;
    QUrl m_url;
};

#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

