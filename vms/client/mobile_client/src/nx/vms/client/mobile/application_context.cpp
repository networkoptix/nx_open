// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <QtCore/QFileSelector>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickWindow>

#include <mobile_client/mobile_client_settings.h>
#include <nx/build_info.h>
#include <nx/utils/serialization/format.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/mobile_client_meta_types.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/discovery/manager.h>

namespace nx::vms::client::mobile {

namespace {

static const QString kQmlRoot = "qrc:///qml/";

core::ApplicationContext::Features makeFeatures()
{
    auto result = core::ApplicationContext::Features::all();
    result.ignoreCustomization = qnSettings->ignoreCustomization();
    return result;
}

} // namespace

struct ApplicationContext::Private
{
    std::unique_ptr<WindowContext> windowContext;
};

ApplicationContext::ApplicationContext(QObject* parent):
    base_type(Mode::mobileClient,
        Qn::SerializationFormat::json,
        nx::vms::api::PeerType::mobileClient,
        qnSettings->customCloudHost(),
        /*customExternalResourceFile*/ {},
        makeFeatures(),
        parent),
    d(new Private{})
{
    initializeMetaTypes();

    initializeNetworkModules();
    const auto selectedLocale = coreSettings()->locale();
    initializeTranslations(selectedLocale.isEmpty()
        ? QLocale::system().name()
        : selectedLocale);

    const auto engine = qmlEngine();

    QStringList selectors;
    QFileSelector fileSelector;
    fileSelector.setExtraSelectors(selectors);
    QQmlFileSelector qmlFileSelector(engine);
    qmlFileSelector.setSelector(&fileSelector);

    engine->setBaseUrl(QUrl(kQmlRoot));
    engine->addImportPath(kQmlRoot);

    if (build_info::isIos())
        engine->addImportPath("qt_qml");

    d->windowContext = std::make_unique<WindowContext>();
    d->windowContext->initializeWindow();
    const auto notifier = qnSettings->notifier(QnMobileClientSettings::ServerTimeMode);
    connect(notifier, &QnPropertyNotifier::valueChanged,
        this, &ApplicationContext::serverTimeModeChanged);

    moduleDiscoveryManager()->start();
}

ApplicationContext::~ApplicationContext()
{
    resetEngine(); // Frees all systems contexts of the QML objects.

    // DesktopCameraConnection is a SystemContextAware object, so it should be deleted before the
    // SystemContext.
    common::ApplicationContext::stopAll();
}

SystemContext* ApplicationContext::currentSystemContext() const
{
    return mainWindowContext()->system()->as<SystemContext>();
}

WindowContext* ApplicationContext::mainWindowContext() const
{
    return d->windowContext.get();
}

void ApplicationContext::closeWindow()
{
    if (!NX_ASSERT(d->windowContext))
        return;

    // Make sure all QML instances are destroyed before window contexts is dissapeared.
    const auto window = d->windowContext->mainWindow();
    window->deleteLater();
    QObject::connect(window, &QObject::destroyed, qApp, &QGuiApplication::quit);
}

bool ApplicationContext::isServerTimeMode() const
{
    return qnSettings->serverTimeMode();
}

void ApplicationContext::setServerTimeMode(bool value)
{
    qnSettings->setServerTimeMode(value);
}

} // namespace nx::vms::client::mobile
