// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_test_setup.h"

#include <QtCore/QSettings>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_startup_parameters.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/skin_image_provider.h>
#include <nx/vms/client/core/utils/font_loader.h>
#include <nx/vms/client/mobile/mobile_client_meta_types.h>
#include <nx/vms/client/mobile/window_context.h>

#include "test_utils.h"

namespace nx::vms::client::mobile::test {

namespace {

/** Makes a mock from the Nx.Test module available in QML as a context property. */
void addMock(QQmlEngine* engine, const QString& propertyName, const char* typeName)
{
    // Loaded from the module, since QML_IMPORT_PATH may override the resources.
    QQmlComponent component(engine);
    component.loadFromModule("Nx.Test", typeName);
    const auto mock = component.create();
    if (!NX_ASSERT(mock, component.errorString()))
        return;

    mock->setParent(engine);
    engine->rootContext()->setContextProperty(propertyName, mock);
}

} // namespace

void QmlTestSetup::applicationAvailable()
{
    // Keep the settings in a temporary directory, since the default one persists between runs.
    NX_ASSERT(m_settingsDir.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());

    QnMobileClientStartupParameters startupParams;
    std::unique_ptr<QnMobileClientSettings> settings(new QnMobileClientSettings());

    m_appContext = std::make_unique<ApplicationContext>(
        startupParams, std::move(settings), core::ApplicationContext::Mode::unitTests);

    initializeQmlTypes();
    TestUtils::registerQmlType();

    qRegisterMetaType<WindowContext*>();

    core::FontLoader::loadFonts(":/fonts");
    QGuiApplication::setFont(QFont("Roboto"));
}

void QmlTestSetup::qmlEngineAvailable(QQmlEngine* engine)
{
    // Adds the resources last, so the modules from QML_IMPORT_PATH take precedence.
    engine->setImportPathList(engine->importPathList() << "qrc:///qml");
    engine->addImageProvider("skin", new core::SkinImageProvider());
    engine->rootContext()->setContextProperty("appContext", m_appContext.get());

    addMock(engine, "windowContext", "WindowContextMock");
    addMock(engine, "mainWindow", "MainWindowMock");
}

void QmlTestSetup::cleanupTestCase()
{
    m_appContext.reset();
}

} // namespace nx::vms::client::mobile::test
