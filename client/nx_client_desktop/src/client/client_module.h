#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client/client_startup_parameters.h>

#include <nx/client/desktop/radass/radass_fwd.h>

#include <nx/utils/singleton.h>

class QGLWidget;
class QnClientCoreModule;
class QnNetworkProxyFactory;
class QnStaticCommonModule;
class QnCloudStatusWatcher;
class QnCameraDataManager;

namespace nx {
namespace client {
namespace desktop {

class AnalyticsMetadataProviderFactory;
class UploadManager;
class WearableManager;

} // namespace desktop
} // namespace client
} // namespace nx

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
    QnCameraDataManager* cameraDataManager() const;

    nx::client::desktop::RadassController* radassController() const;

    QnStartupParameters startupParameters() const;
    nx::client::desktop::UploadManager* uploadManager() const;
    nx::client::desktop::WearableManager* wearableManager() const;

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
    void registerResourceDataProviders();

private:
    QnStartupParameters m_startupParameters;
    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QScopedPointer<QnClientCoreModule> m_clientCoreModule;
    QScopedPointer<nx::client::desktop::AnalyticsMetadataProviderFactory>
        m_analyticsMetadataProviderFactory;
    QnNetworkProxyFactory* m_networkProxyFactory = nullptr;
    QnCloudStatusWatcher* m_cloudStatusWatcher = nullptr;
    QnCameraDataManager* m_cameraDataManager = nullptr;
    nx::client::desktop::RadassController* m_radassController = nullptr;
    nx::client::desktop::UploadManager* m_uploadManager = nullptr;
    nx::client::desktop::WearableManager* m_wearableManager = nullptr;
};

#define qnClientModule QnClientModule::instance()
