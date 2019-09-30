#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client/client_startup_parameters.h>

#include <nx/vms/client/desktop/radass/radass_fwd.h>

#include <nx/utils/singleton.h>

class QOpenGLWidget;
class QnClientCoreModule;
class QnNetworkProxyFactory;
class QnStaticCommonModule;
class QnCloudStatusWatcher;
class QnCameraDataManager;
class QnCommonModule;

namespace nx::vms::client::desktop {

class AnalyticsMetadataProviderFactory;
class UploadManager;
class WearableManager;
class VideoCache;
class ResourceDirectoryBrowser;

} // namespace nx::vms::client::desktop

class NX_VMS_CLIENT_DESKTOP_API QnClientModule: public QObject, public Singleton<QnClientModule>
{
    Q_OBJECT

public:
    explicit QnClientModule(const QnStartupParameters& startupParams, QObject* parent = nullptr);
    virtual ~QnClientModule();

    void initDesktopCamera(QOpenGLWidget* window);
    void startLocalSearchers();

    QnNetworkProxyFactory* networkProxyFactory() const;
    QnCloudStatusWatcher* cloudStatusWatcher() const;
    QnCameraDataManager* cameraDataManager() const;
    QnClientCoreModule* clientCoreModule() const;

    nx::vms::client::desktop::RadassController* radassController() const;

    QnStartupParameters startupParameters() const;
    nx::vms::client::desktop::UploadManager* uploadManager() const;
    nx::vms::client::desktop::WearableManager* wearableManager() const;
    nx::vms::client::desktop::VideoCache* videoCache() const;
    nx::vms::client::desktop::ResourceDirectoryBrowser* resourceDirectoryBrowser() const;

private:
    void initApplication();
    void initThread();
    static void initMetaInfo();
    static void initSurfaceFormat();
    void initSingletons();
    void initRuntimeParams(const QnStartupParameters& startupParams);
    void initLog();
    bool initLogFromFile(const QString& filename, const QString& suffix = QString());
    void initNetwork();
    void initSkin();
    void initLocalResources();
    void initLocalInfo();
    void registerResourceDataProviders();

private:
    // Client module is instantiated with certain components ommited within testing environment.
    // TODO: #vbreus QnClientModule shouldn't be used at all in unit tests ideally.
    bool isTestingEnvironment() const;

private:
    QnStartupParameters m_startupParameters;
    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QScopedPointer<QnClientCoreModule> m_clientCoreModule;
    QScopedPointer<nx::vms::client::desktop::AnalyticsMetadataProviderFactory>
        m_analyticsMetadataProviderFactory;
    QScopedPointer<nx::vms::client::desktop::ResourceDirectoryBrowser> m_resourceDirectoryBrowser;
    QnNetworkProxyFactory* m_networkProxyFactory = nullptr;
    QnCloudStatusWatcher* m_cloudStatusWatcher = nullptr;
    QnCameraDataManager* m_cameraDataManager = nullptr;
    nx::vms::client::desktop::RadassController* m_radassController = nullptr;
    nx::vms::client::desktop::UploadManager* m_uploadManager = nullptr;
    nx::vms::client::desktop::WearableManager* m_wearableManager = nullptr;
    nx::vms::client::desktop::VideoCache* m_videoCache = nullptr;
};

#define qnClientModule QnClientModule::instance()
