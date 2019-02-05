#include "workbench_debug_handler.h"
#include "workbench_debug_handler.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QAction>

#include <QtWebKitWidgets/QWebView>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <nx/vms/api/analytics/engine_manifest.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/style/webview_style.h>
#include <ui/widgets/common/web_page.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/widgets/main_window.h>

#include <nx/vms/client/desktop/ui/dialogs/debug/animations_control_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/debug/applauncher_control_dialog.h>
#include <nx/vms/client/desktop/custom_settings/dialogs/custom_settings_test_dialog.h>
#include <nx/vms/client/desktop/interactive_settings/dialogs/interactive_settings_test_dialog.h>

#include <nx/vms/client/desktop/debug_utils/widgets/palette_widget.h>
#include <nx/vms/client/desktop/debug_utils/dialogs/credentials_store_dialog.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>

#include <finders/test_systems_finder.h>
#include <finders/system_tiles_test_case.h>
#include <finders/systems_finder.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_welcome_screen.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_writers.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/random.h>
#include <nx/utils/range_adapters.h>

#include <nx/fusion/model_functions.h>

//#if defined(_DEBUG)
    #define DEBUG_ACTIONS
//#endif

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

class QnWebViewDialog: public QDialog
{
    using base_type = QDialog;

public:
    QnWebViewDialog(QWidget* parent = nullptr):
        base_type(parent, Qt::Window),
        m_page(new QnWebPage(this)),
        m_webView(new QWebView(this)),
        m_urlLineEdit(new QLineEdit(this))
    {
        m_webView->setPage(m_page);

        NxUi::setupWebViewStyle(m_webView);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(QMargins());
        layout->addWidget(m_urlLineEdit);
        layout->addWidget(m_webView);
        connect(m_urlLineEdit, &QLineEdit::returnPressed, this, [this]()
            {
                m_webView->load(m_urlLineEdit->text());
            });

        auto paletteWidget = new PaletteWidget(this);
        paletteWidget->setDisplayPalette(m_webView->palette());
        layout->addWidget(paletteWidget);
        connect(paletteWidget, &PaletteWidget::paletteChanged, this,
            [this, paletteWidget]
            {
                m_webView->setPalette(paletteWidget->displayPalette());
            });

        }

    QString url() const { return m_urlLineEdit->text(); }

    void setUrl(const QString& value)
    {
        m_urlLineEdit->setText(value);
        m_webView->load(QUrl::fromUserInput(value));
    }

private:
    QnWebPage* m_page;
    QWebView* m_webView;
    QLineEdit* m_urlLineEdit;
};

//-------------------------------------------------------------------------------------------------
// QnDebugControlDialog

class QnDebugControlDialog:
    public QnDialog,
    public QnWorkbenchContextAware
{
    typedef QnDialog base_type;

public:
    QnDebugControlDialog(QWidget* parent):
        base_type(parent),
        QnWorkbenchContextAware(parent)
    {
        using namespace nx::vms::client::desktop::ui;

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->addWidget(newActionButton(action::DebugDecrementCounterAction));
        layout->addWidget(newActionButton(action::DebugIncrementCounterAction));

        auto addButton =
            [this, parent, layout](const QString& name, std::function<void(void)> handler)
            {
                auto button = new QPushButton(name, parent);
                connect(button, &QPushButton::clicked, handler);
                layout->addWidget(button);
            };

        addButton("Applaucher control",
            [this]()
            {
                (new QnApplauncherControlDialog(this, commonModule()->engineVersion()))->show();
            });

        addButton("Animations control",
            [this]() { (new AnimationsControlDialog(this))->show(); });

        addButton("Credentials",
            [this]() { (new CredentialsStoreDialog(this))->show(); });

        addButton("Custom Settings Test",
            [this]() { (new CustomSettingsTestDialog(this))->show(); });

        addButton("Interactive Settings Test",
            [this]()
            {
                const auto dialog = new InteractiveSettingsTestDialog(this);
                dialog->show();
            });

        addButton("Web View",
            [this]()
            {
                auto dialog(new QnWebViewDialog(this));
                //dialog->setUrl("http://localhost:7001");
                dialog->show();
            });

        addButton("Toggle default password",
            [this]()
            {
                const auto cameras = resourcePool()->getAllCameras(QnResourcePtr(), true);
                if (cameras.empty())
                    return;

                const auto caps = Qn::SetUserPasswordCapability | Qn::IsDefaultPasswordCapability;

                const bool isDefaultPassword = cameras.first()->needsToChangeDefaultPassword();

                for (const auto& camera: cameras)
                {
                    // Toggle current option.
                    if (isDefaultPassword)
                        camera->setCameraCapabilities(camera->getCameraCapabilities() & ~caps);
                    else
                        camera->setCameraCapabilities(camera->getCameraCapabilities() | caps);
                    camera->savePropertiesAsync();
                }

            });

        addButton("Palette",
            [this]()
            {
                auto w = new PaletteWidget(this);
                w->setPalette(qApp->palette());
                auto messageBox = new QnMessageBox(mainWindowWidget());
                messageBox->setWindowFlags(Qt::Window);
                messageBox->addCustomWidget(w);
                messageBox->addButton(QDialogButtonBox::Ok);
                messageBox->show();
            });

        addButton("RandomizePtz",
            [this]()
            {
                QList<Ptz::Capabilities> presets;
                presets.push_back(Ptz::NoPtzCapabilities);
                presets.push_back(Ptz::ContinuousZoomCapability);
                presets.push_back(Ptz::ContinuousZoomCapability | Ptz::ContinuousFocusCapability);
                presets.push_back(Ptz::ContinuousZoomCapability | Ptz::ContinuousFocusCapability
                    | Ptz::AuxiliaryPtzCapability);
                presets.push_back(Ptz::ContinuousPanTiltCapabilities);
                presets.push_back(Ptz::ContinuousPtzCapabilities | Ptz::ContinuousFocusCapability
                    | Ptz::AuxiliaryPtzCapability | Ptz::PresetsPtzCapability);

                for (const auto& camera: resourcePool()->getAllCameras(QnResourcePtr(), true))
                {
                    int idx = presets.indexOf(camera->getPtzCapabilities());
                    if (idx < 0)
                        idx = 0;
                    else
                        idx = (idx + 1) % presets.size();

                    camera->setPtzCapabilities(presets[idx]);
                }
            });

        addButton("Resource Pool",
            [this]()
            {
                auto messageBox = new QnMessageBox(mainWindowWidget());
                messageBox->setWindowFlags(Qt::Window);

                messageBox->addCustomWidget(new QnResourceListView(resourcePool()->getResources(),
                    QnResourceListView::SortByNameOption,
                    messageBox));
                messageBox->addButton(QDialogButtonBox::Ok);
                messageBox->show();
            });

        addButton("Tiles tests",
            [this]()
            {
                runTilesTest();
                close();
            });

        // ATTENTION: Analytics-related code below is able to compile, but doesn't work properly.
        // If it is still needed after the change of analytics data storage layout in the database,
        // it should be reworked.
        addButton("Generate Analytics Plugins and Engines",
            [this]()
            {
                const QJsonObject kEngineSettingsModel = QJsonDocument::fromJson(R"json(
                    {
                        "items": [
                            {
                                "type": "GroupBox",
                                "caption": "General",
                                "items": [
                                    {
                                        "type": "TextField",
                                        "name": "description",
                                        "caption": "Description"
                                    },
                                    {
                                        "type": "ComboBox",
                                        "name": "networkType",
                                        "caption": "Neural Network Type",
                                        "defaultValue": "Type 1",
                                        "range": ["Type 1", "Type 2", "Type 3"]
                                    }
                                ]
                            }
                        ]
                    }
                )json").object();

                const QJsonObject kDeviceAgentSettingsModel = QJsonDocument::fromJson(R"json(
                    {
                        "items": [
                            {
                                "type": "GroupBox",
                                "caption": "Detection",
                                "items": [
                                    {
                                        "type": "CheckBox",
                                        "name": "detectFaces",
                                        "caption": "Detect Faces",
                                        "defaultValue": true
                                    },
                                    {
                                        "type": "CheckBox",
                                        "name": "detectPeople",
                                        "caption": "Detect People",
                                        "defaultValue": true
                                    },
                                    {
                                        "type": "CheckBox",
                                        "name": "detectCars",
                                        "caption": "Detect Cars",
                                        "defaultValue": true
                                    }
                                ]
                            }
                        ]
                    }
                )json").object();

                using namespace nx::vms::common;
                using namespace nx::vms::api::analytics;

                auto addPlugin =
                    [&](const QnUuid& id, const QString& name)
                    {
                        PluginManifest manifest;
                        manifest.id = id.toString();
                        manifest.name = name;
                        manifest.engineSettingsModel = kEngineSettingsModel;

                        AnalyticsPluginResourcePtr plugin{
                            new AnalyticsPluginResource(commonModule())};
                        plugin->setId(id);
                        plugin->setName(name);
                        plugin->setManifest(manifest);

                        resourcePool()->addResource(plugin);
                        return plugin;
                    };

                auto addEngine =
                    [&](const QnUuid& id, const QString& name, const QnResourcePtr& parent)
                    {
                        EngineManifest manifest;
                        manifest.deviceAgentSettingsModel = kDeviceAgentSettingsModel;

                        AnalyticsEngineResourcePtr engine{
                            new AnalyticsEngineResource(commonModule())};
                        engine->setId(id);
                        engine->setParentId(parent->getId());
                        engine->setName(name);
                        engine->setManifest(manifest);

                        resourcePool()->addResource(engine);
                        return engine;
                    };

                const auto plugin1 = addPlugin(
                    QnUuid("{c302e227-3631-4ce4-9acc-ed481661ce4d}"), "Plugin 1");
                const auto plugin2 = addPlugin(
                    QnUuid("{fce51681-bc44-4d0c-966b-12f8cf39d2f9}"), "Plugin 2");

                addEngine(QnUuid("{f31e58e7-abc5-4813-ba83-fde0375a98cd}"), "Engine 1", plugin1);
                addEngine(QnUuid("{f31e58e7-abc5-4813-ba83-fde0375a98ce}"), "Engine 2", plugin1);
                addEngine(QnUuid("{f31e58e7-abc5-4813-ba83-fde0375a98cf}"), "Engine 3", plugin2);
            });

        for (auto [name, handler]: nx::utils::constKeyValueRange(debugActions()))
            addButton(name, handler);
    }

private:
    QToolButton* newActionButton(action::IDType actionId, QWidget* parent = nullptr)
    {
        QToolButton* button = new QToolButton(parent);
        button->setDefaultAction(action(actionId));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return button;
    }

    static QnSystemTilesTestCase* s_tilesTests;

    void runTilesTest()
    {
        if (!qnRuntime->isDesktopMode())
            return;

        if (!s_tilesTests)
        {
            static constexpr auto kSomeFarPriority = 1000;
            const auto testSystemsFinder = new QnTestSystemsFinder(qnSystemsFinder);
            qnSystemsFinder->addSystemsFinder(testSystemsFinder, kSomeFarPriority);

            s_tilesTests = new QnSystemTilesTestCase(testSystemsFinder, this);

            const auto welcomeScreen = mainWindow()->welcomeScreen();

            connect(s_tilesTests, &QnSystemTilesTestCase::openTile,
                welcomeScreen, &QnWorkbenchWelcomeScreen::openTile);
            connect(s_tilesTests, &QnSystemTilesTestCase::switchPage,
                welcomeScreen, &QnWorkbenchWelcomeScreen::switchPage);
            connect(s_tilesTests, &QnSystemTilesTestCase::collapseExpandedTile, this,
                [welcomeScreen]() { emit welcomeScreen->openTile(QString()); });
            connect(s_tilesTests, &QnSystemTilesTestCase::restoreApp, this,
                [this]()
                {
                    const auto maximizeAction = action(action::FullscreenAction);
                    if (maximizeAction->isChecked())
                        maximizeAction->toggle();
                });
            connect(s_tilesTests, &QnSystemTilesTestCase::makeFullscreen, this,
                [this]()
                {
                    const auto maximizeAction = action(action::FullscreenAction);
                    if (!maximizeAction->isChecked())
                        maximizeAction->toggle();
                });

            connect(s_tilesTests, &QnSystemTilesTestCase::messageChanged,
                welcomeScreen, &QnWorkbenchWelcomeScreen::setMessage);
        }

        s_tilesTests->runTestSequence(QnTileTest::First);
    }
};

QnSystemTilesTestCase *QnDebugControlDialog::s_tilesTests = nullptr;

} // namespace

//------------------------------------------------------------------------------------------------
// QnWorkbenchDebugHandler

QnWorkbenchDebugHandler::QnWorkbenchDebugHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    #if defined(DEBUG_ACTIONS)
        // TODO: #sivanov #High Remove before release.
        qDebug() << "------------- Debug actions ARE ACTIVE -------------";
        connect(action(action::DebugControlPanelAction), &QAction::triggered, this,
            &QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered);
        connect(action(action::DebugIncrementCounterAction), &QAction::triggered, this,
            &QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered);
        connect(action(action::DebugDecrementCounterAction), &QAction::triggered, this,
            &QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered);
    #endif
}

void QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered()
{
    QnDebugControlDialog* dialog(new QnDebugControlDialog(mainWindowWidget()));
    dialog->show();
}

void QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() + 1);
    qDebug() << qnRuntime->debugCounter();
}

void QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() - 1);
    qDebug() << qnRuntime->debugCounter();
}
