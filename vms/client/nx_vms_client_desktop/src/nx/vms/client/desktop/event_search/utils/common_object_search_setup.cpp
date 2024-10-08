// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_object_search_setup.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>

namespace nx::vms::client::desktop {

struct CommonObjectSearchSetup::Private
{
    WindowContext* windowContext = nullptr;
    nx::utils::ScopedConnections contextConnections;
};

CommonObjectSearchSetup::CommonObjectSearchSetup(QObject* parent):
    CommonObjectSearchSetup(nullptr, parent)
{
}

CommonObjectSearchSetup::CommonObjectSearchSetup(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    d(new Private())
{
    setSystemContext(systemContext);
}

CommonObjectSearchSetup::~CommonObjectSearchSetup()
{
}

WindowContext* CommonObjectSearchSetup::windowContext() const
{
    return d->windowContext;
}

void CommonObjectSearchSetup::setWindowContext(WindowContext* windowContext)
{
    if (d->windowContext == windowContext)
        return;

    d->contextConnections.reset();
    d->windowContext = windowContext;
    setSystemContext(d->windowContext ? d->windowContext->system() : nullptr);
    emit contextChanged();

    if (!d->windowContext)
        return;

    const auto navigator = d->windowContext->navigator();
    d->contextConnections << connect(
        navigator,
        &QnWorkbenchNavigator::timeSelectionChanged,
        this,
        &CommonObjectSearchSetup::handleTimelineSelectionChanged);

    const auto camerasUpdaterFor =
        [this](core::EventSearch::CameraSelection mode)
    {
        return [this, mode]() { updateRelevantCamerasForMode(mode); };
    };

    d->contextConnections
        << connect(d->windowContext->navigator(), &QnWorkbenchNavigator::currentResourceChanged,
               this, camerasUpdaterFor(core::EventSearch::CameraSelection::current));

    d->contextConnections <<
        connect(d->windowContext->workbench(), &Workbench::currentLayoutChanged,
            this, camerasUpdaterFor(core::EventSearch::CameraSelection::layout));

    d->contextConnections <<
        connect(d->windowContext->workbench(), &Workbench::currentLayoutItemsChanged,
            this, camerasUpdaterFor(core::EventSearch::CameraSelection::layout));
}

SystemContext* CommonObjectSearchSetup::systemContext() const
{
    return base_type::systemContext()->as<SystemContext>();
}

void CommonObjectSearchSetup::setSystemContext(SystemContext* systemContext)
{
    base_type::setSystemContext(systemContext);
}

bool CommonObjectSearchSetup::selectCameras(UuidSet& selectedCameras)
{
    return d->windowContext && CameraSelectionDialog::selectCameras<CameraSelectionDialog::DummyPolicy>(
        systemContext(), selectedCameras, d->windowContext->mainWindowWidget());
}

QnVirtualCameraResourcePtr CommonObjectSearchSetup::currentResource() const
{
    if (!d->windowContext)
        return {};

    return d->windowContext->navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
}

QnVirtualCameraResourceSet CommonObjectSearchSetup::currentLayoutCameras() const
{
    if (!d->windowContext)
        return {};

    QnVirtualCameraResourceSet cameras;
    if (auto workbenchLayout = d->windowContext->workbench()->currentLayout())
    {
        for (const auto& item: workbenchLayout->items())
        {
            if (const auto camera = item->resource().dynamicCast<QnVirtualCameraResource>())
                cameras.insert(camera);
        }
    }
    return cameras;
}

void CommonObjectSearchSetup::clearTimelineSelection() const
{
    if (d->windowContext)
        d->windowContext->navigator()->clearTimelineSelection();
}

} // namespace nx::vms::client::desktop
