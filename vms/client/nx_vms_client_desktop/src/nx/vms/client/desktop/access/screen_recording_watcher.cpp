// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "screen_recording_watcher.h"

#include <QtGui/QAction>
#include <QtQml/QtQml>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

ScreenRecordingWatcher::ScreenRecordingWatcher():
    m_screenRecordingAction(appContext()->mainWindowContext()->menu()->action(
        menu::ToggleScreenRecordingAction))
{
    if (!m_screenRecordingAction)
        return;

    connect(m_screenRecordingAction, &QAction::toggled,
        this, &ScreenRecordingWatcher::stateChanged);

    connect(m_screenRecordingAction, &QObject::destroyed,
        this, &ScreenRecordingWatcher::stateChanged);
}

bool ScreenRecordingWatcher::isOn() const
{
    return m_screenRecordingAction && m_screenRecordingAction->isChecked();
}

void ScreenRecordingWatcher::registerQmlType()
{
    qmlRegisterSingletonType<ScreenRecordingWatcher>(
        "nx.vms.client.desktop", 1, 0, "ScreenRecording",
        [](QQmlEngine*, QJSEngine*) { return new ScreenRecordingWatcher(); });
}

} // namespace nx::vms::client::desktop
