// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_context.h"

#include <QtWidgets/QApplication>

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/thumbnails/resource_id_thumbnail.h>
#include <nx/vms/client/core/thumbnails/thumbnail_cache.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>
#include <nx/vms/client/desktop/system_update/server_update_tool.h>
#include <nx/vms/client/desktop/system_update/workbench_update_watcher.h>
#include <nx/vms/client/desktop/thumbnails/live_camera_thumbnail.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/managers/settings_dialog_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/system_settings.h>
#include <statistics/statistics_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/instruments/gl_checker_instrument.h>
#include <ui/statistics/modules/durations_statistics_module.h>
#include <ui/statistics/modules/graphics_statistics_module.h>
#include <ui/statistics/modules/users_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_display.h>

using namespace nx::vms::client::desktop;

QnWorkbenchContext::QnWorkbenchContext(
    WindowContext* windowContext,
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    WindowContextAware(windowContext),
    SystemContextAware(systemContext)
{
    m_userWatcher = systemContext->userWatcher();

    connect(m_userWatcher, &nx::vms::client::core::UserWatcher::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            LiveCameraThumbnail::thumbnailCache()->clear();
            nx::vms::client::core::ResourceIdentificationThumbnail::thumbnailCache()->clear();
            emit userChanged(user);
        });

    /* Create dependent objects. */
    m_settingsDialogManager = std::make_unique<SettingsDialogManager>(this);
    m_display.reset(new QnWorkbenchDisplay(this));
}

void QnWorkbenchContext::initialize()
{
    // Add statistics modules.
    auto statisticsManager = statisticsModule()->manager();

    const auto userStatModule = instance<QnUsersStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("users"), userStatModule);

    const auto graphicsStatModule = instance<QnGraphicsStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("graphics"), graphicsStatModule);

    const auto durationStatModule = instance<QnDurationStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("durations"), durationStatModule);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed,
        statisticsManager, &QnStatisticsManager::resetStatistics);

    instance<QnGLCheckerInstrument>();
    store(new ServerUpdateTool(systemContext()));
    store(new WorkbenchUpdateWatcher(windowContext()));
    instance<ClientUpdateManager>();
    m_resourceTreeSettings.reset(new ResourceTreeSettings());
}

QnWorkbenchContext::~QnWorkbenchContext()
{
    /* Destroy typed subobjects in reverse order to how they were constructed. */
    QnInstanceStorage::clear();

    m_userWatcher = nullptr;

    /* Destruction order of these objects is important. */
    m_resourceTreeSettings.reset();
    m_display.reset();
}

QnWorkbenchDisplay* QnWorkbenchContext::display() const
{
    return m_display.data();
}

SettingsDialogManager* QnWorkbenchContext::settingsDialogManager() const
{
    return m_settingsDialogManager.get();
}

MainWindow* QnWorkbenchContext::mainWindow() const
{
    return m_mainWindow.data();
}

QWidget* QnWorkbenchContext::mainWindowWidget() const
{
    return mainWindow();
}

void QnWorkbenchContext::setMainWindow(MainWindow* mainWindow)
{
    m_mainWindow = mainWindow;
    systemContext()->restApiHelper()->setParent(mainWindow);
}

ResourceTreeSettings* QnWorkbenchContext::resourceTreeSettings() const
{
    return m_resourceTreeSettings.get();
}

nx::core::Watermark QnWorkbenchContext::watermark() const
{
    if (systemSettings()->watermarkSettings().useWatermark
        && user()
        && !accessController()->hasPowerUserPermissions()
        && !user()->getName().isEmpty())
    {
        return {systemSettings()->watermarkSettings(), user()->getName()};
    }
    return {};
}

QnUserResourcePtr QnWorkbenchContext::user() const {
    if(m_userWatcher) {
        return m_userWatcher->user();
    } else {
        return QnUserResourcePtr();
    }
}

bool QnWorkbenchContext::closingDown() const
{
    return m_closingDown;
}

void QnWorkbenchContext::setClosingDown(bool value)
{
    m_closingDown = value;
}

bool QnWorkbenchContext::isWorkbenchVisible() const
{
    return m_mainWindow && m_mainWindow->isWorkbenchVisible();
}
