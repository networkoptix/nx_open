// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_actions_handler.h"

#include <QtGui/QAction>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QPushButton>

#include <api/common_message_processor.h>
#include <client/client_runtime_settings.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/testkit/testkit.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/event/actions/common_action.h>
#include <nx/vms/rules/action_builder_fields/substitution.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_connector.h>
#include <nx/vms/rules/event_filter_fields/keywords_field.h>
#include <nx/vms/rules/events/debug_event.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

#include "../dialogs/animations_control_dialog.h"
#include "../dialogs/applauncher_control_dialog.h"
#include "../dialogs/credentials_store_dialog.h"
#include "../dialogs/custom_settings_test_dialog.h"
#include "../dialogs/interactive_settings_test_dialog.h"
#include "../dialogs/palette_dialog.h"
#include "../dialogs/qml_test_dialog.h"
#include "../dialogs/resource_pool_dialog.h"
#include "../dialogs/web_engine_dialog.h"
#include "../utils/cameras_actions.h"
#include "../utils/client_webserver.h"
#include "../utils/debug_custom_actions.h"

namespace {

using namespace nx::vms::rules;

class DebugEventConnector: public EventConnector
{
public:
    void atInc()
    {
        emit event(EventPtr(new DebugEvent(
            "Increment",
            qnRuntime->debugCounter(),
            qnSyncTime->currentTimePoint())));
    }
    void atDec()
    {
        emit event(EventPtr(new DebugEvent(
            "Decrement",
            qnRuntime->debugCounter(),
            qnSyncTime->currentTimePoint())));
    }
};

std::unique_ptr<DebugEventConnector> s_debugEventConnector;

} // namespace

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

DebugActionsHandler::DebugActionsHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(ui::action::DebugControlPanelAction), &QAction::triggered, this,
        [this]
        {
            auto dialog(new DebugControlPanelDialog(mainWindowWidget()));
            dialog->show();
        });

    if (const int port = ini().clientWebServerPort; port > 0 && port < 65536)
    {
        auto director = context()->instance<DirectorWebserver>();
        QString host = ini().clientWebServerHost;
        director->setListenAddress(host, port);
        bool started = director->start();
        if (!started)
            NX_ERROR(this, "Cannot start client webserver - port %1 already occupied?", port);
    }

    registerDebugCounterActions();
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
    testkit::TestKit::registerAction();
}

DebugActionsHandler::~DebugActionsHandler()
{
    s_debugEventConnector.reset();
}

void DebugActionsHandler::registerDebugCounterActions()
{
    connect(action(ui::action::DebugIncrementCounterAction), &QAction::triggered, this,
        &DebugActionsHandler::at_debugIncrementCounterAction_triggered);

    connect(action(ui::action::DebugDecrementCounterAction), &QAction::triggered, this,
        &DebugActionsHandler::at_debugDecrementCounterAction_triggered);

    s_debugEventConnector = std::make_unique<DebugEventConnector>();
    appContext()->currentSystemContext()->vmsRulesEngine()->addEventConnector(
        s_debugEventConnector.get());

    registerDebugAction(
        "Debug counter ++",
        [](QnWorkbenchContext* context)
        {
            context->menu()->trigger(ui::action::DebugIncrementCounterAction);
        });

    registerDebugAction(
        "Debug counter --",
        [](QnWorkbenchContext* context)
        {
            context->menu()->trigger(ui::action::DebugDecrementCounterAction);
        });
}

void DebugActionsHandler::at_debugIncrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() + 1);
    s_debugEventConnector->atInc();
    NX_INFO(this,
        "+++++++++++++++++++++++++++++++++++++ %1 +++++++++++++++++++++++++++++++++++++",
        qnRuntime->debugCounter());
}

void DebugActionsHandler::at_debugDecrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() - 1);
    s_debugEventConnector->atDec();
    NX_INFO(this,
        "------------------------------------- %1 -------------------------------------",
        qnRuntime->debugCounter());
}

} // namespace nx::vms::client::desktop
