#ifndef QN_MEDIA_SERVER_MODULE_H
#define QN_MEDIA_SERVER_MODULE_H

#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <utils/common/instance_storage.h>

class QnCommonModule;

class QnMediaServerModule: public QObject, public QnInstanceStorage, public Singleton<QnMediaServerModule> {
    Q_OBJECT;
public:
    QnMediaServerModule(const QString& enforcedMediatorEndpoint, QObject *parent = NULL);
    virtual ~QnMediaServerModule();

    using Singleton<QnMediaServerModule>::instance;
    using QnInstanceStorage::instance;
private:
    void initServerMetaTypes();
private:
    QnCommonModule *m_common;
};

#endif // QN_MEDIA_SERVER_MODULE_H
