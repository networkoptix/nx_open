#ifndef QN_MEDIA_SERVER_MODULE_H
#define QN_MEDIA_SERVER_MODULE_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>
#include <utils/common/instance_storage.h>

class QnCommonModule;

class QnMediaServerModule: public QObject, public QnInstanceStorage, public Singleton<QnMediaServerModule> {
    Q_OBJECT;
public:
    QnMediaServerModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnMediaServerModule();

    using Singleton<QnMediaServerModule>::instance;
    using QnInstanceStorage::instance;
private:
    QnCommonModule *m_common;
};

#endif // QN_MEDIA_SERVER_MODULE_H
