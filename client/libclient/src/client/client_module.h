#pragma once

#include <QtCore/QObject>

#include <client/client_startup_parameters.h>

#include <nx/utils/singleton.h>

class QGLWidget;
class QnClientCoreModule;

class QnClientModule: public QObject, public Singleton<QnClientModule>
{
    Q_OBJECT

public:
    QnClientModule(const QnStartupParameters &startupParams = QnStartupParameters(), QObject *parent = NULL);
    virtual ~QnClientModule();

    void initDesktopCamera(QGLWidget* window);
    void startLocalSearchers();

private:
    void initApplication();
    void initThread();
    void initMetaInfo();
    void initSingletons     (const QnStartupParameters& startupParams);
    void initRuntimeParams  (const QnStartupParameters& startupParams);
    void initLog            (const QnStartupParameters& startupParams);
    void initNetwork        (const QnStartupParameters& startupParams);
    void initSkin           (const QnStartupParameters& startupParams);
    void initLocalResources (const QnStartupParameters& startupParams);

private:
    QnClientCoreModule* m_clientCoreModule;
};

#define qnClientModule QnClientModule::instance();
