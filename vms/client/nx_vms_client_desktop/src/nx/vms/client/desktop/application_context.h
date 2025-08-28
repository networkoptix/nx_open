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

namespace nx::cloud::gateway { class VmsGatewayEmbeddable; }
namespace nx::vms::client::core { class ObjectDisplaySettings; }
namespace nx::speech_synthesizer{ class TextToWaveServer; }

namespace nx::vms::client::desktop {

class ClientStateHandler;
class ContextStatisticsModule;
class FontConfig;
class LocalSettings;
class PerformanceMonitor;
class RadassController;
class ResourceFactory;
class ResourceDirectoryBrowser;
class ResourcesChangesManager;
class RunningInstancesManager;
class ScreenRecordingSettings;
class SharedMemoryManager;
class ShowOnceSettings;
class MessageBarSettings;
class SystemContext;
class UploadManager;
class WebPageDataCache;
class WindowContext;

namespace joystick { class Manager; }

/**
 * Main context of the desktop client application. Exists through all application lifetime and is
 * accessible from anywhere using appContext function.
 */
class NX_VMS_CLIENT_DESKTOP_API ApplicationContext: public core::ApplicationContext
{
    Q_OBJECT
    using base_type = core::ApplicationContext;

public:
    enum class Mode
    {
        /** Generic Desktop Client mode. */
        desktopClient,

        /** Desktop Client self-update mode (with admin permissions). */
        selfUpdate,

        /** Unit tests mode. */
        unitTests,
    };

    enum class FeatureFlag
    {
        none = 0,
        // No additional features currently.
        all = -1
    };
    Q_DECLARE_FLAGS(FeatureFlags, FeatureFlag)

    struct Features
    {
        core::ApplicationContext::Features core;
        FeatureFlags flags = FeatureFlag::none;

        Features() = default;
        Features(
            core::ApplicationContext::Features core,
            FeatureFlags flags)
            :
            core(core),
            flags(flags)
        {
        }

        static Features all()
        {
            return {
                core::ApplicationContext::Features::all(),
                FeatureFlag::all
            };
        }

        static Features none()
        {
            return {
                core::ApplicationContext::Features::none(),
                FeatureFlag::none
            };
        }
    };

    ApplicationContext(
        Mode mode,
        Features features,
        const QnStartupParameters& startupParameters,
        QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    virtual core::SystemContext* createSystemContext(
        core::SystemContext::Mode mode, QObject* parent = nullptr) override;

    /**
     * Version of the application. Can be overridden with command-line parameters.
     */
    nx::utils::SoftwareVersion version() const;

    /**
     * Context of the System we are currently connected to. Also contains local files.
     */
    SystemContext* currentSystemContext() const;

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
    virtual nx::Uuid peerId() const override;

    /**
     * @return Id of the current Video Wall instance if the client was run in the Video Wall mode.
     */
    nx::Uuid videoWallInstanceId() const;

    /**
     * Central place for the initialization, storage and access to various statistics modules.
     */
    ContextStatisticsModule* statisticsModule() const;

    /**
     * Local Client settings.
     */
    LocalSettings* localSettings() const;

    /** Runtime Client instance settings. */
    QnClientRuntimeSettings* runtimeSettings() const;

    ScreenRecordingSettings* screenRecordingSettings() const;

    /** Provider of input audio data from the system microphone. */
    virtual std::unique_ptr<QnAbstractStreamDataProvider> createAudioInputProvider() const override;

    ShowOnceSettings* showOnceSettings() const;
    MessageBarSettings* messageBarSettings() const;

    /**
     * Map of analytics objects colors by object type. Persistently stored on a PC.
     */
    core::ObjectDisplaySettings* objectDisplaySettings() const;

    ClientStateHandler* clientStateHandler() const;

    /** Interface for IPC between client instances. */
    SharedMemoryManager* sharedMemoryManager() const;

    /** Set custom Shared Memory Manager implementation. */
    void setSharedMemoryManager(std::unique_ptr<SharedMemoryManager> value);

    RunningInstancesManager* runningInstancesManager() const;

    /**
     * Monitors system resources usage: CPU, memory, GPU, etc, in its own separate thread.
     */
    PerformanceMonitor* performanceMonitor() const;

    RadassController* radassController() const;

    ResourceFactory* resourceFactory() const;

    UploadManager* uploadManager() const;

    QnResourceDiscoveryManager* resourceDiscoveryManager() const;

    ResourceDirectoryBrowser* resourceDirectoryBrowser() const;

    ResourcesChangesManager* resourcesChangesManager() const;

    QnForgottenSystemsManager* forgottenSystemsManager() const;

    nx::speech_synthesizer::TextToWaveServer* textToWaveServer() const;

    WebPageDataCache* webPageDataCache() const;

    nx::cloud::gateway::VmsGatewayEmbeddable* cloudGateway() const;

    FontConfig* fontConfig() const;

    joystick::Manager* joystickManager() const;

    const QnStartupParameters& startupParameters() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/**
 * Main context of the desktop client application. Exists through all application lifetime.
 */
inline ApplicationContext* appContext()
{
    return common::ApplicationContext::instance()->as<ApplicationContext>();
}

} // namespace nx::vms::client::desktop
