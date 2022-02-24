// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "welcome_screen_test.h"

#include <client/client_runtime_settings.h>
#include <finders/system_tiles_test_case.h>
#include <finders/systems_finder.h>
#include <finders/test_systems_finder.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>

#include "debug_custom_actions.h"

namespace nx::vms::client::desktop {

QnSystemTilesTestCase* s_tilesTests = nullptr;

void runTilesTest(QnWorkbenchContext* context)
{
    if (!qnRuntime->isDesktopMode())
        return;

    if (!s_tilesTests)
    {
        static constexpr auto kSomeFarPriority = 1000;
        const auto testSystemsFinder = new QnTestSystemsFinder(qnSystemsFinder);
        qnSystemsFinder->addSystemsFinder(testSystemsFinder, kSomeFarPriority);

        s_tilesTests = new QnSystemTilesTestCase(testSystemsFinder, context);

        const auto welcomeScreen = context->mainWindow()->welcomeScreen();

        QObject::connect(s_tilesTests, &QnSystemTilesTestCase::openTile, welcomeScreen,
            [welcomeScreen](const QString& systemId) { welcomeScreen->openTile(systemId); });
        QObject::connect(s_tilesTests, &QnSystemTilesTestCase::switchPage, welcomeScreen,
            &WelcomeScreen::switchPage);
        QObject::connect(s_tilesTests, &QnSystemTilesTestCase::collapseExpandedTile, welcomeScreen,
            [welcomeScreen]() { emit welcomeScreen->openTile(QString()); });
        QObject::connect(s_tilesTests, &QnSystemTilesTestCase::restoreApp, context,
            [context]()
            {
                const auto maximizeAction = context->menu()->action(ui::action::FullscreenAction);
                if (maximizeAction->isChecked())
                    maximizeAction->toggle();
            });
        QObject::connect(s_tilesTests, &QnSystemTilesTestCase::makeFullscreen, welcomeScreen,
            [context]()
            {
                const auto maximizeAction = context->menu()->action(ui::action::FullscreenAction);
                if (!maximizeAction->isChecked())
                    maximizeAction->toggle();
            });

        QObject::connect(s_tilesTests,
            &QnSystemTilesTestCase::messageChanged,
            welcomeScreen,
            &WelcomeScreen::setMessage);
    }

    s_tilesTests->runTestSequence(QnTileTest::First);
}

void WelcomeScreenTest::registerAction()
{
    registerDebugAction(
        "Tiles tests",
        [](QnWorkbenchContext* context)
        {
            runTilesTest(context);
        });
}

} // namespace nx::vms::client::desktop {
