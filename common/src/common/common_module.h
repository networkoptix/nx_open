#ifndef QN_COMMON_MODULE_H
#define QN_COMMON_MODULE_H

#include <QtCore/QObject>

class QnPtzMapperPool;
class QnSessionManager;

/**
 * Storage for common module's global state.
 * 
 * All singletons and initialization/deinitialization code goes here.
 */
class QnCommonModule: public QObject {
    Q_OBJECT
public:
    QnCommonModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnCommonModule();

    /**
     * \returns                         Global instance of common module.
     *                                  Note that this instance must be created first (e.g. on the stack, like a <tt>QApplication</tt>).
     */
    static QnCommonModule *instance() {
        return s_instance;
    }

    QnPtzMapperPool *ptzMapperPool() const {
        return m_ptzMapperPool;
    }

    QnSessionManager *sessionManager() const {
        return m_sessionManager;
    }

private:
    static QnCommonModule *s_instance;
    QnPtzMapperPool *m_ptzMapperPool;
    QnSessionManager *m_sessionManager;
};

#define qnCommon (QnCommonModule::instance())

#endif // QN_COMMON_MODULE_H

