// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_actions_handler.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QAction>

#include <api/common_message_processor.h>
#include <client/client_runtime_settings.h>
#include <common/common_module.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/event/actions/common_action.h>
#include <nx/vms/rules/action_executor.h>
#include <nx/vms/rules/action_fields/substitution.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_connector.h>
#include <nx/vms/rules/event_fields/keywords.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/workbench/workbench_context.h>

#include "../dialogs/animations_control_dialog.h"
#include "../dialogs/applauncher_control_dialog.h"
#include "../dialogs/credentials_store_dialog.h"
#include "../dialogs/custom_settings_test_dialog.h"
#include "../dialogs/interactive_settings_test_dialog.h"
#include "../dialogs/qml_test_dialog.h"
#include "../dialogs/palette_dialog.h"
#include "../dialogs/resource_pool_dialog.h"
#include "../dialogs/web_engine_dialog.h"
#include "../utils/cameras_actions.h"
#include "../utils/client_webserver.h"
#include "../utils/debug_custom_actions.h"
#include "../utils/welcome_screen_test.h"

namespace {

using namespace nx::vms::rules;

//#define FIELD(type, getter, setter) \
//public: \
//  type getter() const { return m_##getter; } \
//  void setter(const type &val) { m_##getter = val; } \
//private: \
//  type m_##getter;
//
//class DebugEvent: public BasicEvent
//{
//    Q_PROPERTY(QString action READ action)
//    Q_PROPERTY(qint64 value READ value)
//
//    FIELD(QString, test, setTest)
//
//public:
//    DebugEvent(const QString &action, qint64 value):
//        BasicEvent("DebugEvent"),
//        m_action(action),
//        m_value(value)
//    {
//    }
//
//    QString action() const
//    {
//        return m_action;
//    }
//
//    qint64 value() const
//    {
//        return m_value;
//    }
//
//private:
//    QString m_action;
//    qint64 m_value;
//};

class DebugEventConnector:
    public EventConnector,
    public Singleton<DebugEventConnector>
{
public:
    void atInc()
    {
        emit ruleEvent(EventPtr(new DebugEvent("Increment", qnRuntime->debugCounter())));
    }
    void atDec()
    {
        emit ruleEvent(EventPtr(new DebugEvent("Decrement", qnRuntime->debugCounter())));
    }
};

class DebugActionExecutor:
    public ActionExecutor,
    public QnCommonModuleAware,
    public Singleton<DebugActionExecutor>
{
public:
    DebugActionExecutor(QnCommonModule* commonModule): QnCommonModuleAware(commonModule){}

    virtual void execute(const ActionPtr& action) override
    {
        // Assuming that now we have only one action type...
        auto notif = (NotificationAction*)action.data();

        nx::vms::event::EventParameters runtimeParams;
        runtimeParams.eventType = nx::vms::api::EventType::userDefinedEvent;
        runtimeParams.caption = notif->caption();
        runtimeParams.description = notif->description();
        nx::vms::event::AbstractActionPtr oldAction(
            nx::vms::event::CommonAction::create(
                nx::vms::api::ActionType::showPopupAction, runtimeParams));
        auto params = oldAction->getParams();
        params.allUsers = true;
        oldAction->setParams(params);

        emit commonModule()->messageProcessor()->businessActionReceived(oldAction);
    }
};

} // namespace

template<> DebugEventConnector* Singleton<DebugEventConnector>::s_instance = nullptr;
template<> DebugActionExecutor* Singleton<DebugActionExecutor>::s_instance = nullptr;

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

    connect(action(ui::action::DebugIncrementCounterAction), &QAction::triggered, this,
        &DebugActionsHandler::at_debugIncrementCounterAction_triggered);

    connect(action(ui::action::DebugDecrementCounterAction), &QAction::triggered, this,
        &DebugActionsHandler::at_debugDecrementCounterAction_triggered);

    if (const int port = ini().clientWebServerPort; port > 0 && port < 65536)
    {
        auto director = context()->instance<DirectorWebserver>();
        QString host = ini().clientWebServerHost;
        director->setListenAddress(host, port);
        bool started = director->start();
        if (!started)
            NX_ERROR(this, "Cannot start client webserver - port %1 already occupied?", port);
    }

    auto engine = commonModule()->vmsRulesEngine();
    auto connector = new DebugEventConnector(); // initialize instance
    auto executor = new DebugActionExecutor(commonModule()); // initialize instance
    engine->addEventConnector(connector);
    engine->addActionExecutor("nx.showNotification", executor);
    engine->registerActionType("nx.showNotification", [](){ return new NotificationAction(); });

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
    WelcomeScreenTest::registerAction();
}

void DebugActionsHandler::at_debugIncrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() + 1);
    DebugEventConnector::instance()->atInc();
    NX_INFO(this,
        "+++++++++++++++++++++++++++++++++++++ %1 +++++++++++++++++++++++++++++++++++++",
        qnRuntime->debugCounter());
}

void DebugActionsHandler::at_debugDecrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() - 1);
    DebugEventConnector::instance()->atDec();
    NX_INFO(this,
        "------------------------------------- %1 -------------------------------------",
        qnRuntime->debugCounter());
}

} // namespace nx::vms::client::desktop
