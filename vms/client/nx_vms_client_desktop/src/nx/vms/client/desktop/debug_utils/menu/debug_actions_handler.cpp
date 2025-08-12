// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_actions_handler.h"

#include <QtGui/QAction>
#include <QtQml/QQmlEngine>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QPushButton>

#include <api/common_message_processor.h>
#include <api/server_rest_connection.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <nx/build_info.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/rules/event_test_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/testkit/testkit.h>
#include <nx/vms/common/system_settings.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

#include "../dialogs/animations_control_dialog.h"
#include "../dialogs/applauncher_control_dialog.h"
#include "../dialogs/credentials_store_dialog.h"
#include "../dialogs/custom_settings_test_dialog.h"
#include "../dialogs/interactive_settings_test_dialog.h"
#include "../dialogs/joystick_investigation_wizard/joystick_investigation_wizard_dialog.h"
#include "../dialogs/name_value_table_test_dialog.h"
#include "../dialogs/palette_dialog.h"
#include "../dialogs/qml_test_dialog.h"
#include "../dialogs/resource_pool_dialog.h"
#include "../dialogs/sorting_test_dialog.h"
#include "../dialogs/web_engine_dialog.h"
#include "../utils/cameras_actions.h"
#include "../utils/client_webserver.h"
#include "../utils/debug_custom_actions.h"

#if defined Q_OS_MAC
    #include "../dialogs/virtual_joystick_dialog_mac.h"
#endif

namespace nx::vms::client::desktop {

namespace {

//-------------------------------------------------------------------------------------------------
// DebugControlPanelDialog

class DebugControlPanelDialog:
    public QnDialog,
    public QnWorkbenchContextAware
{
public:
    DebugControlPanelDialog(QWidget* parent):
        QnDialog(parent),
        QnWorkbenchContextAware(parent)
    {
        auto layout = new QVBoxLayout(this);

        const auto& actions = debugActions();
        for (auto it = actions.cbegin(); it != actions.cend(); ++it)
        {
            auto button = new QPushButton(it->first, parent);
            auto handler = it->second;
            connect(button, &QPushButton::clicked,
                [this, handler] { handler(this->context()); });
            layout->addWidget(button);
        }
    }
};

} // namespace

//------------------------------------------------------------------------------------------------
// DebugActionsHandler

struct DebugActionsHandler::Private
{
    Private(WindowContext* windowContext)
    {
        // Command line argument overrides ini parameter.
        int port = appContext()->startupParameters().webServerPort;
        if (port <= 0 || port >= 65536)
            port = ini().clientWebServerPort;

        if (port > 0 && port < 65536)
        {
            // FIXME: #amalov Some parts of the director may go to the app context.
            director = std::make_unique<Director>(windowContext);
            director->setupJSEngine(appContext()->qmlEngine());

            directorServer = std::make_unique<DirectorWebserver>(director.get());

            QString host = ini().clientWebServerHost;
            directorServer->setListenAddress(host, port);

            bool started = directorServer->start();
            if (!started)
                NX_ERROR(this, "Cannot start client webserver - port %1 already occupied?", port);
        }
    }

    std::unique_ptr<Director> director;
    std::unique_ptr<DirectorWebserver> directorServer;
};

DebugActionsHandler::DebugActionsHandler(WindowContext* windowContext, QObject *parent):
    base_type(parent),
    WindowContextAware(windowContext),
    d(new Private(windowContext))
{
    connect(action(menu::DebugControlPanelAction), &QAction::triggered, this,
        [this]
        {
            auto dialog(new DebugControlPanelDialog(mainWindowWidget()));
            dialog->show();
        });

    registerDebugCounterActions();
    registerDebugAction("Crash the Client", [](auto) { NX_CRITICAL(false); });

    AnimationsControlDialog::registerAction();
    ApplauncherControlDialog::registerAction();
    CamerasActions::registerAction();
    CredentialsStoreDialog::registerAction();
    CustomSettingsTestDialog::registerAction();
    InteractiveSettingsTestDialog::registerAction();
    PaletteDialog::registerAction();
    QmlTestDialog::registerAction();
    ResourcePoolDialog::registerAction();
    WebEngineDialog::registerAction();
    SortingTestDialog::registerAction();
    NameValueTableTestDialog::registerAction();
    rules::EventTestDialog::registerAction();

#ifdef Q_OS_MAC
    if (ini().virtualJoystick)
        VirtualJoystickDialog::registerAction();
#endif

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    if (ini().joystickInvestigationWizard)
        JoystickInvestigationWizardDialog::registerAction();
#endif

    connect(action(menu::DebugToggleElementHighlight), &QAction::triggered, this,
        [](bool checked)
        {
            if (auto testkit = testkit::TestKit::instance())
                testkit->setHighlightEnabled(checked);
        });

    connect(action(menu::DebugToggleSecurityForPowerUsersAction), &QAction::triggered,
        this, &DebugActionsHandler::enableSecurityForPowerUsers);

    const auto updateSecurityForPowerUsers =
        [this]()
        {
            action(menu::DebugToggleSecurityForPowerUsersAction)->setChecked(
                system()->globalSettings()->securityForPowerUsers());
        };

    connect(
        system()->globalSettings(),
        &nx::vms::common::SystemSettings::securityForPowerUsersChanged,
        this,
        updateSecurityForPowerUsers);

    updateSecurityForPowerUsers();
}

DebugActionsHandler::~DebugActionsHandler()
{
}

void DebugActionsHandler::registerDebugCounterActions()
{
    connect(action(menu::DebugIncrementCounterAction), &QAction::triggered, this,
        &DebugActionsHandler::at_debugIncrementCounterAction_triggered);

    connect(action(menu::DebugDecrementCounterAction), &QAction::triggered, this,
        &DebugActionsHandler::at_debugDecrementCounterAction_triggered);

    registerDebugAction(
        "Debug counter ++",
        [this](QnWorkbenchContext* /*context*/)
        {
            menu()->trigger(menu::DebugIncrementCounterAction);
        });

    registerDebugAction(
        "Debug counter --",
        [this](QnWorkbenchContext* /*context*/)
        {
            menu()->trigger(menu::DebugDecrementCounterAction);
        });
}

void DebugActionsHandler::enableSecurityForPowerUsers(bool value)
{
    const auto api = system()->connectedServerApi();
    if (!api)
        return;

    const auto handler =
        [this, oldValue = !value](bool success,
            rest::Handle /*requestId*/,
            rest::ServerConnection::ErrorOrEmpty /*result*/)
        {
            if (success)
            {
                QnMessageBox::information(mainWindowWidget(),
                    nx::format("Security settings for Power Users have been successfully %1",
                        oldValue ? "disabled" : "enabled"));
            }
            else
            {
                action(menu::DebugToggleSecurityForPowerUsersAction)->setChecked(oldValue);

                QnMessageBox::critical(mainWindowWidget(),
                    nx::format("Failed to %1 security settings for Power Users",
                        oldValue ? "disable" : "enable"));
            }
        };

    const auto request = QString("/rest/v3/system/settings/securityForPowerUsers");
    const auto tokenHelper = system()->restApiHelper()->getSessionTokenHelper();
    const auto paramValue = value ? "true" : "false";

    api->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        tokenHelper,
        nx::network::http::Method::put,
        request,
        network::rest::Params{},
        paramValue,
        handler,
        thread());
}

void DebugActionsHandler::at_debugIncrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() + 1);
    NX_INFO(this,
        "+++++++++++++++++++++++++++++++++++++ %1 +++++++++++++++++++++++++++++++++++++",
        qnRuntime->debugCounter());
}

void DebugActionsHandler::at_debugDecrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() - 1);
    NX_INFO(this,
        "------------------------------------- %1 -------------------------------------",
        qnRuntime->debugCounter());
}

} // namespace nx::vms::client::desktop
