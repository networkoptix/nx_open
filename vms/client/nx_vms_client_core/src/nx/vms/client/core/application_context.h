// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/serialization/format.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/application_context.h>

Q_MOC_INCLUDE("nx/vms/client/core/network/cloud_status_watcher.h")

class QQmlEngine;

class QnAbstractStreamDataProvider;
class QnClientCoreSettings;
class QnResourcePool;

namespace nx::i18n { class TranslationManager; }
namespace nx::vms::discovery { class Manager; }

namespace nx::vms::client::core {

class ColorTheme;
class CloudCrossSystemManager;
class CloudLayoutsManager;
class CloudStatusWatcher;
class FontConfig;
class NetworkModule;
class Skin;
class Settings;
class SystemFinder;
class ThreadPool;
class UnifiedResourcePool;
class VoiceSpectrumAnalyzer;
class SessionTokenTerminator;

namespace watchers { class KnownServerConnections; }

/**
 * Main context of the desktop client application. Exists through all application lifetime and is
 * accessible from anywhere using `appContext function.
 */
class NX_VMS_CLIENT_CORE_API ApplicationContext: public common::ApplicationContext
{
    Q_OBJECT

    Q_PROPERTY(CloudStatusWatcher* cloudStatusWatcher
        READ cloudStatusWatcher
        CONSTANT)

public:
    enum class Mode
    {
        desktopClient,
        mobileClient,
        unitTests
    };

    enum class FeatureFlag
    {
        none = 0,
        qml = 1 << 0,
        cross_site = 1 << 1,

        all = -1
    };
    Q_DECLARE_FLAGS(FeatureFlags, FeatureFlag)

    struct Features
    {
        common::ApplicationContext::Features base;
        bool ignoreCustomization = false;
        FeatureFlags flags = FeatureFlag::none;

        Features() = default;
        Features(
            common::ApplicationContext::Features base,
            FeatureFlags flags)
            :
            base(base),
            flags(flags)
        {
        }

        static Features all()
        {
            return {
                common::ApplicationContext::Features::all(),
                FeatureFlag::all
            };
        }

        static Features none()
        {
            return {
                common::ApplicationContext::Features::none(),
                FeatureFlag::none
            };
        }
    };

    ApplicationContext(
        Mode mode,
        Qn::SerializationFormat serializationFormat,
        PeerType peerType = PeerType::notDefined,
        Features features = Features::none(),
        const QString& customCloudHost = {},
        const QString& customExternalResourceFile = {},
        QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    Qn::SerializationFormat serializationFormat() const;

    /**
     * Initialize network-related modules.
     * Not called from the constructor, should be explicitly called from descendants instead.
     */
    void initializeNetworkModules();

    /**
     * Initialize cross-system-related modules.
     * Not called from the constructor, should be explicitly called from descendants instead.
     */
    void initializeCrossSystemModules();

    QQmlEngine* qmlEngine() const;

    CloudStatusWatcher* cloudStatusWatcher() const;

    nx::vms::discovery::Manager* moduleDiscoveryManager() const;

    SystemFinder* systemFinder() const;

    VoiceSpectrumAnalyzer* voiceSpectrumAnalyzer() const;

    Settings* coreSettings() const;

    watchers::KnownServerConnections* knownServerConnectionsWatcher() const;

    Skin* skin() const;

    ColorTheme* colorTheme() const;

    /**
     * Unified interface to access all available Resource Pools.
     */
    UnifiedResourcePool* unifiedResourcePool() const;

    virtual SystemContext* createSystemContext(
        SystemContext::Mode mode, QObject* parent = nullptr);

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

    // Temporary workaround: needed until we have a single main SystemContext for all connections.
    void addMainContext(SystemContext* mainContext);

    /**
     * Unregister existing system context before destroying.
     */
    void removeSystemContext(SystemContext* systemContext);

    /**
     * Find existing System Context by its cloud system id.
     */
    SystemContext* systemContextByCloudSystemId(const QString& cloudSystemId) const;

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

    FontConfig* fontConfig() const;

    virtual nx::Uuid peerId() const;

    /**
     * Obtain a named thread pool.
     * This is the preferred way (instead of explicitly creating a QThreadPool), as these pools are
     * owned by the application context, and destruction of them and their threads happens at
     * a safe moment (before QCoreApplication destruction).
     */
    ThreadPool* threadPool(const QString& id) const;

    SessionTokenTerminator* sessionTokenTerminator() const;

    virtual bool isCertificateValidationLevelStrict() const override;

    /** Provider of input audio data from the system microphone. */
    virtual std::unique_ptr<QnAbstractStreamDataProvider> createAudioInputProvider() const;

    NetworkModule* networkModule() const;

signals:
    void systemContextAdded(SystemContext* systemContext);
    void systemContextRemoved(SystemContext* systemContext);

protected:

    void storeFontConfig(FontConfig* config);

    void resetEngine();

    /**
     *  Initializes translation-related stuff. Should be called on the inheritor side since
     *  we need some migration/initializations to be done before translations setup.
     */
    void initializeTranslations(const QString& targetLocale);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/**
 * Main context of the client application. Exists through all application lifetime.
 */
inline ApplicationContext* appContext()
{
    return common::ApplicationContext::instance()->as<ApplicationContext>();
}

} // namespace nx::vms::client::core
