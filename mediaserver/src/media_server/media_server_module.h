#ifndef QN_MEDIA_SERVER_MODULE_H
#define QN_MEDIA_SERVER_MODULE_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>

class QnCommonModule;

class QnMediaServerModule: public QObject, public Singleton<QnMediaServerModule> {
    Q_OBJECT;
public:
    QnMediaServerModule(int &argc, char **argv, QObject *parent = NULL);
    virtual ~QnMediaServerModule();

private:
    QnCommonModule *m_common;
};

#endif // QN_MEDIA_SERVER_MODULE_H
