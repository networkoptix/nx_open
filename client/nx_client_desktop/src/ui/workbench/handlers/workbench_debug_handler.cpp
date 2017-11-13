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

#include <nx/api/analytics/driver_manifest.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/widgets/palette_widget.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/common/web_page.h>
#include <ui/widgets/views/resource_list_view.h>

#include <nx/client/desktop/ui/dialogs/debug/animations_control_dialog.h>
#include <nx/client/desktop/ui/dialogs/debug/applauncher_control_dialog.h>

#include <finders/test_systems_finder.h>
#include <finders/system_tiles_test_case.h>
#include <finders/systems_finder.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_welcome_screen.h>

#include <nx/api/analytics/driver_manifest.h>
#include <nx/api/analytics/supported_events.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/random.h>

//#ifdef _DEBUG
#define DEBUG_ACTIONS
//#endif

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

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(QMargins());
        layout->addWidget(m_urlLineEdit);
        layout->addWidget(m_webView);
        connect(m_urlLineEdit, &QLineEdit::returnPressed, this, [this]()
            {
                m_webView->load(m_urlLineEdit->text());
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
    QnDebugControlDialog(QWidget *parent = NULL):
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

        addButton(lit("Web View"), [this]
            {
                auto dialog(new QnWebViewDialog(this));
                dialog->setUrl(lit("http://localhost:7001"));
                dialog->show();
            });

        addButton(lit("Toggle default password"),
            [this]
            {
                const auto cameras = resourcePool()->getAllCameras(QnResourcePtr(), true);
                if (cameras.empty())
                    return;

                const auto caps = Qn::SetUserPasswordCapability | Qn::isDefaultPasswordCapability;

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
                QnPaletteWidget *w = new QnPaletteWidget(this);
                w->setPalette(qApp->palette());
                auto messageBox = new QnMessageBox(mainWindow());
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
                auto messageBox = new QnMessageBox(mainWindow());
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

        addButton(lit("Generate analytics manifests"),
            [this]()
            {
                auto servers = resourcePool()->getAllServers(Qn::AnyStatus);
                if (servers.empty())
                    return;

                int serverIndex = 0;

                for (int i = 0; i < 5; ++i)
                {
                    nx::api::AnalyticsDriverManifest manifest;
                    manifest.driverId = QnUuid::createUuid();
                    manifest.driverName.value = lit("Driver %1").arg(i);
                    manifest.driverName.localization[lit("ru_RU")] = lit("Russian %1").arg(i);
                    for (int j = 0; j < 3; ++j)
                    {
                        nx::api::AnalyticsEventType eventType;
                        eventType.eventTypeId = QnUuid::createUuid();
                        eventType.eventName.value = lit("Event %1").arg(j);
                        eventType.eventName.localization[lit("ru_RU")] = lit("Russion %1").arg(j);
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
                    drivers.push_back(nx::api::AnalyticsDriverManifest()); //< Some cameras will not have driver.

                    for (auto camera: resourcePool()->getAllCameras(server, true))
                    {
                        const auto randomDriver = nx::utils::random::choice(drivers);
                        if (randomDriver.driverId.isNull()) //< dummy driver
                        {
                            camera->setAnalyticsSupportedEvents({});
                        }
                        else
                        {
                            nx::api::AnalyticsSupportedEvents supported;
                            std::transform(randomDriver.outputEventTypes.cbegin(),
                                randomDriver.outputEventTypes.cend(),
                                std::back_inserter(supported),
                                [](const nx::api::AnalyticsEventType& eventType)
                                {
                                    return eventType.eventTypeId;
                                });
                            camera->setAnalyticsSupportedEvents(supported);
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
                    camera->setAnalyticsSupportedEvents({});
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

            const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();

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

    auto supressLog = [](const QString& tag)
        {
            const auto logger = nx::utils::log::addLogger({tag});
            logger->setDefaultLevel(nx::utils::log::Level::none);
            logger->setWriter(std::make_unique<nx::utils::log::StdOut>());
        };

    auto consoleLog = [](const QString& tag)
        {
            const auto logger = nx::utils::log::addLogger({tag});
            logger->setDefaultLevel(nx::utils::log::Level::verbose);
            logger->setWriter(std::make_unique<nx::utils::log::StdOut>());
        };

    supressLog(lit("__freeSlot"));
    supressLog(lit("__workbenchState"));
    supressLog(lit("__itemMap"));
    supressLog(QnLog::PERMISSIONS_LOG);
    //consoleLog(lit("nx::client::desktop::RadassController::Private"));
}

void QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered()
{
    QnDebugControlDialog* dialog(new QnDebugControlDialog(mainWindow()));
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
