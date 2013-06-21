#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>
#include <utils/common/instance_storage.h>

class QnSessionManager;

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

    QnSessionManager *sessionManager() const {
        return m_sessionManager;
    }

    using Singleton<QnCommonModule>::instance;
    using QnInstanceStorage::instance;

private:
    QnSessionManager *m_sessionManager;
};

#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

