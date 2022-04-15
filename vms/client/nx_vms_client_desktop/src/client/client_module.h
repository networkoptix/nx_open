// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client/client_startup_parameters.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>
#include <nx/vms/client/desktop/radass/radass_fwd.h>

class QOpenGLWidget;
class QnClientCoreModule;
class QnStaticCommonModule;
class QnCameraDataManager;

namespace nx::vms::api { enum class PeerType; }

namespace nx::vms::license {

class VideoWallLicenseUsageHelper;

} // namespace nx::vms::license

namespace nx::vms::utils { class TranslationManager; }

namespace nx::vms::client::desktop {

class AnalyticsAttributeHelper;
class AnalyticsMetadataProviderFactory;
class AnalyticsSettingsManager;
class ClientStateHandler;
class LicenseHealthWatcher;
class PerformanceMonitor;
class ResourceDirectoryBrowser;
class RunningInstancesManager;
class ServerRuntimeEventConnector;
class SharedMemoryManager;
class SystemContext;
class UploadManager;
class VideoCache;
class VirtualCameraManager;

namespace session {
class SessionManager;
class DefaultProcessInterface;
} // namespace session

namespace analytics {
class TaxonomyManager;
class AttributeHelper;
} // namespace analytics

} // namespace nx::vms::client::desktop

class NX_VMS_CLIENT_DESKTOP_API QnClientModule: public QObject, public Singleton<QnClientModule>
{
    Q_OBJECT

public:
    explicit QnClientModule(
        const QnStartupParameters& startupParams,
        QObject* parent = nullptr);
    virtual ~QnClientModule();

    void initDesktopCamera(QOpenGLWidget* window);
    void startLocalSearchers();

    QnCameraDataManager* cameraDataManager() const;
    QnClientCoreModule* clientCoreModule() const;

    nx::vms::client::desktop::SystemContext* systemContext() const;

    nx::vms::client::desktop::session::SessionManager* sessionManager() const;
    nx::vms::client::desktop::ClientStateHandler* clientStateHandler() const;
    nx::vms::client::desktop::SharedMemoryManager* sharedMemoryManager() const;
    nx::vms::client::desktop::RunningInstancesManager* runningInstancesManager() const;

    nx::vms::client::desktop::RadassController* radassController() const;

    QnStartupParameters startupParameters() const;
    nx::vms::client::desktop::UploadManager* uploadManager() const;
    nx::vms::client::desktop::VirtualCameraManager* virtualCameraManager() const;
    nx::vms::client::desktop::VideoCache* videoCache() const;
    nx::vms::client::desktop::ResourceDirectoryBrowser* resourceDirectoryBrowser() const;
    nx::vms::client::desktop::AnalyticsSettingsManager* analyticsSettingsManager() const;
    nx::vms::client::desktop::ServerRuntimeEventConnector* serverRuntimeEventConnector() const;

    nx::vms::utils::TranslationManager* translationManager() const;

    nx::vms::client::desktop::PerformanceMonitor* performanceMonitor() const;

    nx::vms::license::VideoWallLicenseUsageHelper* videoWallLicenseUsageHelper() const;
    nx::vms::client::desktop::analytics::TaxonomyManager* taxonomyManager() const;
    nx::vms::client::desktop::analytics::AttributeHelper* analyticsAttributeHelper() const;

    static void initWebEngine();

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
    void initLocalInfo(nx::vms::api::PeerType peerType);
    void registerResourceDataProviders();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QScopedPointer<QnClientCoreModule> m_clientCoreModule;
    QScopedPointer<nx::vms::client::desktop::AnalyticsMetadataProviderFactory>
        m_analyticsMetadataProviderFactory;
    QScopedPointer<nx::vms::client::desktop::ResourceDirectoryBrowser> m_resourceDirectoryBrowser;

    QScopedPointer<nx::vms::client::desktop::session::DefaultProcessInterface> m_processInterface;
    using SessionManager = nx::vms::client::desktop::session::SessionManager;
    QScopedPointer<SessionManager> m_sessionManager;
    QnCameraDataManager* m_cameraDataManager = nullptr;
    nx::vms::client::desktop::RadassController* m_radassController = nullptr;
    nx::vms::client::desktop::UploadManager* m_uploadManager = nullptr;
    nx::vms::client::desktop::VirtualCameraManager* m_virtualCameraManager = nullptr;
    nx::vms::client::desktop::VideoCache* m_videoCache = nullptr;
    QScopedPointer<nx::vms::client::desktop::PerformanceMonitor> m_performanceMonitor;
    QScopedPointer<nx::vms::client::desktop::LicenseHealthWatcher> m_licenseHealthWatcher;
};

#define qnClientModule QnClientModule::instance()
