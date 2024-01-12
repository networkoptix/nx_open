// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/application_context.h>

class QQmlEngine;

class QnClientCoreSettings;
class QnSystemsFinder;
class QnVoiceSpectrumAnalyzer;

namespace nx::vms::discovery { class Manager; }

namespace nx::vms::client::core {

class ColorTheme;
class CloudStatusWatcher;
class FontConfig;
class Settings;

namespace watchers { class KnownServerConnections; }

/**
 * Main context of the desktop client application. Exists through all application lifetime and is
 * accessible from anywhere using `instance()` method.
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
     * Main context of the desktop client application. Exists through all application lifetime.
     */
    static ApplicationContext* instance();

    /**
     * Initialize network-related modules.
     */
    void initializeNetworkModules();

    QQmlEngine* qmlEngine() const;

    CloudStatusWatcher* cloudStatusWatcher() const;

    nx::vms::discovery::Manager* moduleDiscoveryManager() const;

    QnSystemsFinder* systemsFinder() const;

    QnVoiceSpectrumAnalyzer* voiceSpectrumAnalyzer() const;

    Settings* coreSettings() const;

    ColorTheme* colorTheme() const;

    watchers::KnownServerConnections* knownServerConnectionsWatcher() const;

    FontConfig* fontConfig() const;

protected:
    void storeFontConfig(FontConfig* config);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

inline ApplicationContext* appContext() { return ApplicationContext::instance(); }

} // namespace nx::vms::client::core
