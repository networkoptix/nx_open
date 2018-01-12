#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client/client_startup_parameters.h>

#include <nx/utils/singleton.h>

class QGLWidget;
class QnClientCoreModule;
class QnNetworkProxyFactory;
class QnStaticCommonModule;
class QnCloudStatusWatcher;
class QnClientUploadManager;
class QnClientWearableManager;

class QnClientModule: public QObject, public Singleton<QnClientModule>
{
    Q_OBJECT

public:
    QnClientModule(const QnStartupParameters &startupParams = QnStartupParameters(), QObject *parent = NULL);
    virtual ~QnClientModule();

    void initDesktopCamera(QGLWidget* window);
    void startLocalSearchers();

    QnNetworkProxyFactory* networkProxyFactory() const;
    QnCloudStatusWatcher* cloudStatusWatcher() const;
    QnClientUploadManager* uploadManager() const;
    QnClientWearableManager* wearableManager() const;

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
    void initLocalInfo(const QnStartupParameters& startupParams);

private:
    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QScopedPointer<QnClientCoreModule> m_clientCoreModule;
    QnNetworkProxyFactory* m_networkProxyFactory;
    QnCloudStatusWatcher* m_cloudStatusWatcher;
    QnClientUploadManager* m_uploadManager;
    QnClientWearableManager* m_wearableManager;
};

#define qnClientModule QnClientModule::instance()
