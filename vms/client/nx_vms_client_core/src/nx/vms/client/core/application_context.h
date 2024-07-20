// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/application_context.h>

class QQmlEngine;

class QnClientCoreSettings;
class QnVoiceSpectrumAnalyzer;

namespace nx::i18n { class TranslationManager; }
namespace nx::vms::discovery { class Manager; }

namespace nx::vms::client::core {

class ColorTheme;
class CloudStatusWatcher;
class FontConfig;
class Skin;
class Settings;
class SystemContext;
class SystemFinder;
class ThreadPool;
class UnifiedResourcePool;

namespace watchers { class KnownServerConnections; }

/**
 * Main context of the desktop client application. Exists through all application lifetime and is
 * accessible from anywhere using `appContext function.
 */
class NX_VMS_CLIENT_CORE_API ApplicationContext: public common::ApplicationContext
{
    Q_OBJECT

public:
    enum class Mode
    {
        desktopClient,
        mobileClient,
        unitTests
    };

    ApplicationContext(
        Mode mode,
        PeerType peerType,
        const QString& customCloudHost,
        bool ignoreCustomization,
        const QString& customExternalResourceFile = {},
        QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    /**
     * Initialize network-related modules.
     */
    void initializeNetworkModules();

    QQmlEngine* qmlEngine() const;

    CloudStatusWatcher* cloudStatusWatcher() const;

    nx::vms::discovery::Manager* moduleDiscoveryManager() const;

    SystemFinder* systemFinder() const;

    QnVoiceSpectrumAnalyzer* voiceSpectrumAnalyzer() const;

    Settings* coreSettings() const;

    watchers::KnownServerConnections* knownServerConnectionsWatcher() const;

    Skin* skin() const;

    ColorTheme* colorTheme() const;

    /**
     * Unified interface to access all available Resource Pools.
     */
    UnifiedResourcePool* unifiedResourcePool() const;

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

    nx::i18n::TranslationManager* translationManager() const;

    FontConfig* fontConfig() const;

    /**
     * Obtain a named thread pool.
     * This is the preferred way (instead of explicitly creating a QThreadPool), as these pools are
     * owned by the application context, and destruction of them and their threads happens at
     * a safe moment (before QCoreApplication destruction).
     */
    ThreadPool* threadPool(const QString& id) const;

signals:
    void systemContextAdded(SystemContext* systemContext);
    void systemContextRemoved(SystemContext* systemContext);

protected:
    void addMainContext(SystemContext* mainContext);

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
