// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context.h"

#include <QtCore/QPointer>

#include <nx/vms/client/desktop/system_tab_bar/system_tab_bar_model.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/watchers/workbench_layout_aspect_ratio_watcher.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

struct WindowContext::Private
{
    QPointer<QnWorkbenchContext> workbenchContext;

    std::unique_ptr<QnWorkbenchRenderWatcher> resourceWidgetRenderWatcher;
    std::unique_ptr<QnWorkbenchLayoutAspectRatioWatcher> layoutAspectRatioWatcher;
    std::unique_ptr<QnWorkbenchStreamSynchronizer> streamSynchronizer;
    std::unique_ptr<SystemTabBarModel> systemTabBarModel;
};

WindowContext::WindowContext(QnWorkbenchContext* workbenchContext, QObject* parent):
    QObject(parent),
    d(new Private())
{
    d->workbenchContext = workbenchContext;
    d->resourceWidgetRenderWatcher = std::make_unique<QnWorkbenchRenderWatcher>(this);

    // Depends on resourceWidgetRenderWatcher.
    d->layoutAspectRatioWatcher = std::make_unique<QnWorkbenchLayoutAspectRatioWatcher>(this);

    // Depends on resourceWidgetRenderWatcher.
    d->streamSynchronizer = std::make_unique<QnWorkbenchStreamSynchronizer>(this);

    d->systemTabBarModel = std::make_unique<SystemTabBarModel>(this);
}

WindowContext::~WindowContext()
{
}

QWidget* WindowContext::mainWindowWidget() const
{
    return d->workbenchContext->mainWindowWidget();
}

QnWorkbenchContext* WindowContext::workbenchContext() const
{
    return d->workbenchContext.data();
}

QnWorkbenchRenderWatcher* WindowContext::resourceWidgetRenderWatcher() const
{
    return d->resourceWidgetRenderWatcher.get();
}

QnWorkbenchStreamSynchronizer* WindowContext::streamSynchronizer() const
{
    return d->streamSynchronizer.get();
}

SystemTabBarModel* WindowContext::systemTabBarModel() const
{
    return d->systemTabBarModel.get();
}

} // namespace nx::vms::client::desktop
