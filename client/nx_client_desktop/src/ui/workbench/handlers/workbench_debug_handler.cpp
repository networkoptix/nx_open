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

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/style/webview_style.h>
#include <ui/widgets/common/web_page.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/widgets/main_window.h>

#include <nx/client/desktop/ui/dialogs/debug/animations_control_dialog.h>
#include <nx/client/desktop/ui/dialogs/debug/applauncher_control_dialog.h>
#include <nx/client/desktop/custom_settings/dialogs/custom_settings_test_dialog.h>
#include <nx/client/desktop/interactive_settings/dialogs/interactive_settings_test_dialog.h>
#include <nx/client/desktop/debug_utils/widgets/palette_widget.h>

#include <finders/test_systems_finder.h>
#include <finders/system_tiles_test_case.h>
#include <finders/systems_finder.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_welcome_screen.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_writers.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/random.h>
#include <nx/client/desktop/debug_utils/dialogs/credentials_store_dialog.h>

//#ifdef _DEBUG
#define DEBUG_ACTIONS
//#endif

using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

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

// -------------------------------------------------------------------------- //
// QnDebugControlDialog
// -------------------------------------------------------------------------- //
class QnDebugControlDialog: public QnDialog, public QnWorkbenchContextAware
{
    typedef QnDialog base_type;

public:
    QnDebugControlDialog(QWidget *parent):
        base_type(parent),
        QnWorkbenchContextAware(parent)
    {
        using namespace nx::client::desktop::ui;

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(newActionButton(action::DebugDecrementCounterAction));
        layout->addWidget(newActionButton(action::DebugIncrementCounterAction));

        auto addButton = [this, parent, layout]
            (const QString& name, std::function<void(void)> handler)
            {
                auto button = new QPushButton(name, parent);
                connect(button, &QPushButton::clicked, handler);
                layout->addWidget(button);
            };

        addButton(lit("Applaucher control"), [this] { (new QnApplauncherControlDialog(this))->show();});

        addButton(lit("Animations control"), [this] { (new AnimationsControlDialog(this))->show(); });

        addButton("Credentials", [this]{ (new CredentialsStoreDialog(this))->show(); });

        addButton(lit("Custom Settings Test"), [this]
            { (new CustomSettingsTestDialog(this))->show(); });

        addButton(lit("Interactive Settings Test"),
            [this]
            {
                const auto dialog = new InteractiveSettingsTestDialog(this);
                dialog->show();
            });

        addButton(lit("Web View"), [this]
            {
                auto dialog(new QnWebViewDialog(this));
                //dialog->setUrl(lit("http://localhost:7001"));
                dialog->show();
            });

        addButton(lit("Toggle default password"),
            [this]
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
                    camera->saveParamsAsync();
                }

            });

        addButton(lit("Palette"), [this]
            {
                auto w = new PaletteWidget(this);
                w->setPalette(qApp->palette());
                auto messageBox = new QnMessageBox(mainWindowWidget());
                messageBox->setWindowFlags(Qt::Window);
                messageBox->addCustomWidget(w);
                messageBox->addButton(QDialogButtonBox::Ok);
                messageBox->show();
            });

        addButton(lit("RandomizePtz"), [this]
            {
                QList<Ptz::Capabilities> presets;
                presets.push_back(Ptz::NoPtzCapabilities);
                presets.push_back(Ptz::ContinuousZoomCapability);
                presets.push_back(Ptz::ContinuousZoomCapability | Ptz::ContinuousFocusCapability);
                presets.push_back(Ptz::ContinuousZoomCapability | Ptz::ContinuousFocusCapability
                    | Ptz::AuxilaryPtzCapability);
                presets.push_back(Ptz::ContinuousPanTiltCapabilities);
                presets.push_back(Ptz::ContinuousPtzCapabilities | Ptz::ContinuousFocusCapability
                    | Ptz::AuxilaryPtzCapability | Ptz::PresetsPtzCapability);

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

        addButton(lit("Resource Pool"), [this]
            {
                auto messageBox = new QnMessageBox(mainWindowWidget());
                messageBox->setWindowFlags(Qt::Window);

                messageBox->addCustomWidget(new QnResourceListView(resourcePool()->getResources(),
                    QnResourceListView::SortByNameOption,
                    messageBox));
                messageBox->addButton(QDialogButtonBox::Ok);
                messageBox->show();
            });

        addButton(lit("Tiles tests"),
            [this]()
            {
                runTilesTest();
                close();
            });

        addButton("Generate Analytics Plugins and Engines",
            [this]()
            {
                const QString kEngineSettingsModel = R"json(
                {
                    "items":
                    [
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
                })json";

                const QString kDeviceAgentSettingsModel = R"json(
                {
                    "items":
                    [
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
                })json";

                using namespace nx::vms::common;

                QnResourcePtr plugin1(new AnalyticsPluginResource(commonModule()));
                plugin1->setId(QnUuid("{c302e227-3631-4ce4-9acc-ed481661ce4d}"));
                plugin1->setName("Plugin1");
                plugin1->setProperty(AnalyticsPluginResource::kEngineSettingsModelProperty,
                    kEngineSettingsModel);
                plugin1->setProperty(AnalyticsPluginResource::kDeviceAgentSettingsModelProperty,
                    kDeviceAgentSettingsModel);

                QnResourcePtr plugin2(new AnalyticsPluginResource(commonModule()));
                plugin2->setId(QnUuid("{fce51681-bc44-4d0c-966b-12f8cf39d2f9}"));
                plugin2->setName("Plugin2");
                plugin2->setProperty(AnalyticsPluginResource::kEngineSettingsModelProperty,
                    kEngineSettingsModel);
                plugin2->setProperty(AnalyticsPluginResource::kDeviceAgentSettingsModelProperty,
                    kDeviceAgentSettingsModel);

                QnResourcePtr engine1(new AnalyticsEngineResource(commonModule()));
                engine1->setId(QUuid("{f31e58e7-abc5-4813-ba83-fde0375a98cd}"));
                engine1->setParentId(plugin1->getId());
                engine1->setName("Engine1");

                QnResourcePtr engine2(new AnalyticsEngineResource(commonModule()));
                engine2->setParentId(plugin1->getId());
                engine2->setId(QUuid("{f31e58e7-abc5-4813-ba83-fde0375a98ce}"));
                engine2->setName("Engine2");

                QnResourcePtr engine3(new AnalyticsEngineResource(commonModule()));
                engine3->setParentId(plugin2->getId());
                engine3->setId(QUuid("{f31e58e7-abc5-4813-ba83-fde0375a98cf}"));
                engine3->setName("Engine3");

                resourcePool()->addResource(plugin1);
                resourcePool()->addResource(plugin2);
                resourcePool()->addResource(engine1);
                resourcePool()->addResource(engine2);
                resourcePool()->addResource(engine3);
            });

        addButton(lit("Generate analytics manifests"),
            [this]()
            {
                auto servers = resourcePool()->getAllServers(Qn::AnyStatus);
                if (servers.empty())
                    return;

                int serverIndex = 0;

                for (int i = 0; i < 5; ++i)
                {
                    nx::vms::api::analytics::EngineManifest manifest;
                    manifest.pluginId = lit("nx.generatedDriver.%1").arg(i);
                    manifest.pluginName.value = lit("Plugin %1").arg(i);
                    manifest.pluginName.localization[lit("ru_RU")] = lit("Russian %1").arg(i);
                    for (int j = 0; j < 3; ++j)
                    {
                        nx::vms::api::analytics::EventType eventType;
                        eventType.id = "";
                        eventType.name.value = lit("Event %1").arg(j);
                        eventType.name.localization[lit("ru_RU")] = lit("Russian %1").arg(j);
                        manifest.outputEventTypes.push_back(eventType);
                    }

                    auto server = servers[serverIndex];
                    auto manifests = server->analyticsDrivers();
                    manifests.push_back(manifest);
                    server->setAnalyticsDrivers(manifests);
                    server->saveParamsAsync();

                    serverIndex = (serverIndex + 1) % servers.size();
                }

                for (auto server: servers)
                {
                    auto drivers = server->analyticsDrivers();
                    // Some devices will not have an Engine.
                    drivers.push_back(nx::vms::api::analytics::EngineManifest());

                    for (auto camera: resourcePool()->getAllCameras(server, true))
                    {
                        const auto randomDriver = nx::utils::random::choice(drivers);
                        if (randomDriver.pluginId.isEmpty()) //< dummy driver
                        {
                            camera->setSupportedAnalyticsEventTypeIds({});
                        }
                        else
                        {
                            QnSecurityCamResource::AnalyticsEventTypeIds eventTypeIds;
                            std::transform(randomDriver.outputEventTypes.cbegin(),
                                randomDriver.outputEventTypes.cend(),
                                std::back_inserter(eventTypeIds),
                                [](const nx::vms::api::analytics::EventType& eventType)
                                {
                                    return eventType.id;
                                });
                            camera->setSupportedAnalyticsEventTypeIds(eventTypeIds);
                        }

                        camera->saveParamsAsync();
                    }
                }
            });

        addButton(lit("Clear analytics manifests"),
            [this]()
            {
                for (auto camera: resourcePool()->getAllCameras({}))
                {
                    camera->setSupportedAnalyticsEventTypeIds({});
                    camera->saveParamsAsync();
                }

                for (auto server: resourcePool()->getAllServers(Qn::AnyStatus))
                {
                    server->setAnalyticsDrivers({});
                    server->saveParamsAsync();
                }
            });


    }

private:
    QToolButton *newActionButton(action::IDType actionId, QWidget *parent = NULL)
    {
        QToolButton *button = new QToolButton(parent);
        button->setDefaultAction(action(actionId));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return button;
    }

    static QnSystemTilesTestCase *m_tilesTests;

    void runTilesTest()
    {
        if (!qnRuntime->isDesktopMode())
            return;

        if (!m_tilesTests)
        {
            static constexpr auto kSomeFarPriority = 1000;
            const auto testSystemsFinder = new QnTestSystemsFinder(qnSystemsFinder);
            qnSystemsFinder->addSystemsFinder(testSystemsFinder, kSomeFarPriority);

            m_tilesTests = new QnSystemTilesTestCase(testSystemsFinder, this);

            const auto welcomeScreen = mainWindow()->welcomeScreen();

            connect(m_tilesTests, &QnSystemTilesTestCase::openTile,
                welcomeScreen, &QnWorkbenchWelcomeScreen::openTile);
            connect(m_tilesTests, &QnSystemTilesTestCase::switchPage,
                welcomeScreen, &QnWorkbenchWelcomeScreen::switchPage);
            connect(m_tilesTests, &QnSystemTilesTestCase::collapseExpandedTile, this,
                [welcomeScreen]() { emit welcomeScreen->openTile(QString());});
            connect(m_tilesTests, &QnSystemTilesTestCase::restoreApp, this,
                [this]()
                {
                    const auto maximizeAction = action(action::FullscreenAction);
                    if (maximizeAction->isChecked())
                        maximizeAction->toggle();
                });
            connect(m_tilesTests, &QnSystemTilesTestCase::makeFullscreen, this,
                [this]()
                {
                    const auto maximizeAction = action(action::FullscreenAction);
                    if (!maximizeAction->isChecked())
                        maximizeAction->toggle();
                });

            connect(m_tilesTests, &QnSystemTilesTestCase::messageChanged,
                welcomeScreen, &QnWorkbenchWelcomeScreen::setMessage);
        }

        m_tilesTests->runTestSequence(QnTileTest::First);
    }
};

QnSystemTilesTestCase *QnDebugControlDialog::m_tilesTests = nullptr;


} // namespace

// -------------------------------------------------------------------------- //
// QnWorkbenchDebugHandler
// -------------------------------------------------------------------------- //
QnWorkbenchDebugHandler::QnWorkbenchDebugHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
#ifdef DEBUG_ACTIONS
    // TODO: #GDM #High remove before release
    qDebug() << "------------- Debug actions ARE ACTIVE -------------";
    connect(action(action::DebugControlPanelAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered);
    connect(action(action::DebugIncrementCounterAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered);
    connect(action(action::DebugDecrementCounterAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered);
#endif

    auto supressLog = [](const nx::utils::log::Tag& tag)
        {
            nx::utils::log::addLogger(
                std::make_unique<nx::utils::log::Logger>(
                    std::set<nx::utils::log::Tag>{tag},
                    nx::utils::log::Level::none,
                    std::make_unique<nx::utils::log::StdOut>()));
        };

    auto consoleLog = [](const nx::utils::log::Tag& tag)
        {
            nx::utils::log::addLogger(
                std::make_unique<nx::utils::log::Logger>(
                    std::set<nx::utils::log::Tag>{tag},
                    nx::utils::log::Level::verbose,
                    std::make_unique<nx::utils::log::StdOut>()));
        };

    // Constants kWorkbenchStateTag, kItemMapTag and kFreeSlotTag should be used instead.
    supressLog(nx::utils::log::Tag(lit("__freeSlot")));
    supressLog(nx::utils::log::Tag(lit("__workbenchState")));
    supressLog(nx::utils::log::Tag(lit("__itemMap")));
    supressLog(QnLog::PERMISSIONS_LOG);
    //consoleLog(lit("nx::client::desktop::RadassController::Private"));
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
