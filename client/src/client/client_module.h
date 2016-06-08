#ifndef QN_CLIENT_MODULE_H
#define QN_CLIENT_MODULE_H

#include <QtCore/QObject>

#include <client/client_startup_parameters.h>

class QnClientModule: public QObject
{
    Q_OBJECT

public:
    QnClientModule(const QnStartupParameters &startupParams = QnStartupParameters(), QObject *parent = NULL);
    virtual ~QnClientModule();

private:
    void initThread();
    void initApplication();
    void initMetaInfo();
    void initSingletons     (const QnStartupParameters& startupParams);
    void initRuntimeParams  (const QnStartupParameters& startupParams);
    void initLog            (const QnStartupParameters& startupParams);
    void initNetworkSockets (const QnStartupParameters& startupParams);
    void initSkin           (const QnStartupParameters& startupParams);
};


#endif // QN_CLIENT_MODULE_H
