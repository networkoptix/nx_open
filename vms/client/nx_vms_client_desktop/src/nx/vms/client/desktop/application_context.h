// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/utils/software_version.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/application_context.h>

class QOpenGLWidget;

class QnResourceDiscoveryManager;
class QnResourcePool;

class QnClientCoreModule;
class QnForgottenSystemsManager;
class QnClientRuntimeSettings;
struct QnStartupParameters;

namespace nx::vms::utils { class TranslationManager; }
namespace nx::cloud::gateway { class VmsGatewayEmbeddable; }

namespace nx::vms::client::desktop {

class ClientStateHandler;
class CloudCrossSystemManager;
class CloudLayoutsManager;
class ContextStatisticsModule;
class LocalSettings;
class ObjectDisplaySettings;
class PerformanceMonitor;
class RadassController;
class ResourceFactory;
class ResourcesChangesManager;
class RunningInstancesManager;
class ScreenRecordingSettings;
class SharedMemoryManager;
class ShowOnceSettings;
class MessageBarSettings;
class SystemContext;
class UnifiedResourcePool;
class UploadManager;
class WebPageIconCache;
class WindowContext;

namespace session {
class SessionManager;
class DefaultProcessInterface;
} // namespace session

/**
 * Main context of the desktop client application. Exists through all application lifetime and is
 * accessible from anywhere using `instance()` method.
 */
class NX_VMS_CLIENT_DESKTOP_API ApplicationContext: public core::ApplicationContext
{
    Q_OBJECT

public:
    enum class Mode
    {
        /** Generic Desktop Client mode. */
        desktopClient,

        /** Dekstop Client self-update mode (with admin permissions). */
        selfUpdate,

        /** Unit tests mode. */
        unitTests,
    };

    ApplicationContext(
        Mode mode,
        const QnStartupParameters& startupParameters,
        QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    /**
     * Main context of the desktop client application. Exists through all application lifetime.
     */
    static ApplicationContext* instance();

    /**
     * Version of the application. Can be overridden with command-line parameters.
     */
    nx::utils::SoftwareVersion version() const;

    /**
     * Context of the System we are currently connected to. Also contains local files.
     */
    SystemContext* currentSystemContext() const;

    /**
     * Contexts of the all Systems for which we have established connection.
     */
    std::vector<SystemContext*> systemContexts() const;

    /**
     * Register existing System Context. First registered is considered to be the Current Context.
     * Ownership is not passed, but Context is to be unregistered before being destroyed.
     * TODO: #sivanov Pass ownership here and emit signals on adding / deleting Contexts.
     * TODO: @sivanov Allow to change Current Context later.
     */
    void addSystemContext(SystemContext* systemContext);

    /**
     * Unregister existing system context before destroying.
     */
    void removeSystemContext(SystemContext* systemContext);

    /**
     * Find existing System Context by it's cloud system id.
     */
    SystemContext* systemContextByCloudSystemId(const QString& cloudSystemId) const;

    /**
     * Main Window context.
     */
    WindowContext* mainWindowContext() const;

    /**
     * Register existing Window Context. First registered is considered to be the Main Window.
     * Ownership is not passed, but Context is to be unregistered before being destroyed.
     * TODO: #sivanov Pass ownership here and emit signals on adding / deleting Contexts.
     * TODO: @sivanov Allow to change Main Window later.
     */
    void addWindowContext(WindowContext* windowContext);

    /**
     * Unregister existing Window Context before destroying.
     */
    void removeWindowContext(WindowContext* windowContext);

    /**
     * Initialize desktop camera implementation depending on the current OS. For Windows it will be
     * the screen recording camera, for other OS - audio only.
     */
    void initializeDesktopCamera(QOpenGLWidget* window);

    /**
     * @return Id of the current peer in the Message Bus. It is persistent and is not changed
     *     between the application runs. Desktop Client calculates actual peer id depending on the
     *     stored persistent id and on the number of the running client instance, so different
     *     Client windows have different peer ids.
     */
    QnUuid peerId() const;

    /**
     * @return Id of the current Video Wall instance if the client was run in the Video Wall mode.
     */
    QnUuid videoWallInstanceId() const;

    /**
     * Temporary accessor until QnClientCoreModule contents is moved to the corresponding
     * contexts.
     */
    QnClientCoreModule* clientCoreModule() const;

    /**
     * Central place for the initialization, storage and access to various statistics modules.
     */
    ContextStatisticsModule* statisticsModule() const;

    /**
     * Unified interface to access all available Resource Pools.
     */
    UnifiedResourcePool* unifiedResourcePool() const;

    /**
     * Local Client settings.
     */
    LocalSettings* localSettings() const;

    /** Runtime Client instance settings. */
    QnClientRuntimeSettings* runtimeSettings() const;

    ScreenRecordingSettings* screenRecordingSettings() const;

    ShowOnceSettings* showOnceSettings() const;
    MessageBarSettings* messageBarSettings() const;

    /**
     * Map of analytics objects colors by object type. Persistently stored on a PC.
     */
    ObjectDisplaySettings* objectDisplaySettings() const;

    ClientStateHandler* clientStateHandler() const;

    /** Interface for IPC between client instances. */
    SharedMemoryManager* sharedMemoryManager() const;

    /** Set custom Shared Memory Manager implementation. */
    void setSharedMemoryManager(std::unique_ptr<SharedMemoryManager> value);

    RunningInstancesManager* runningInstancesManager() const;
    session::SessionManager* sessionManager() const;

    nx::vms::utils::TranslationManager* translationManager() const;

    /**
     * Monitors available Cloud Systems and manages corresponding cross-system Contexts.
     */
    CloudCrossSystemManager* cloudCrossSystemManager() const;

    /**
     * Monitors available Cloud Layouts.
     */
    CloudLayoutsManager* cloudLayoutsManager() const;

    /**
     * System context, containing Cloud Layouts.
     */
    SystemContext* cloudLayoutsSystemContext() const;
    QnResourcePool* cloudLayoutsPool() const;

    /**
     * Monitors system resources usage: CPU, memory, GPU, etc, in its own separate thread.
     */
    PerformanceMonitor* performanceMonitor() const;

    RadassController* radassController() const;

    ResourceFactory* resourceFactory() const;

    UploadManager* uploadManager() const;

    QnResourceDiscoveryManager* resourceDiscoveryManager() const;

    ResourcesChangesManager* resourcesChangesManager() const;

    QnForgottenSystemsManager* forgottenSystemsManager() const;

    WebPageIconCache* webPageIconCache() const;

    nx::cloud::gateway::VmsGatewayEmbeddable* cloudGateway() const;

signals:
    void systemContextAdded(SystemContext* systemContext);
    void systemContextRemoved(SystemContext* systemContext);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

inline ApplicationContext* appContext() { return ApplicationContext::instance(); }

} // namespace nx::vms::client::desktop
