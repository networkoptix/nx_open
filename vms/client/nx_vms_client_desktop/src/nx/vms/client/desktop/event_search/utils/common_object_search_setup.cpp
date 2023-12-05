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
    WindowContext* context = nullptr;
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
    base_type::setContext(systemContext);
}

CommonObjectSearchSetup::~CommonObjectSearchSetup()
{
}

WindowContext* CommonObjectSearchSetup::context() const
{
    return d->context;
}

void CommonObjectSearchSetup::setContext(WindowContext* value)
{
    if (d->context == value)
        return;

    d->contextConnections.reset();
    d->context = value;
    base_type::setContext(d->context ? d->context->system() : nullptr);
    emit contextChanged();

    if (!d->context)
        return;

    const auto navigator = d->context->navigator();
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
        << connect(d->context->navigator(), &QnWorkbenchNavigator::currentResourceChanged,
               this, camerasUpdaterFor(core::EventSearch::CameraSelection::current));

    d->contextConnections <<
        connect(d->context->workbench(), &Workbench::currentLayoutChanged,
            this, camerasUpdaterFor(core::EventSearch::CameraSelection::layout));

    d->contextConnections <<
        connect(d->context->workbench(), &Workbench::currentLayoutItemsChanged,
            this, camerasUpdaterFor(core::EventSearch::CameraSelection::layout));
}

bool CommonObjectSearchSetup::selectCameras(QnUuidSet& selectedCameras)
{
    return d->context && CameraSelectionDialog::selectCameras<CameraSelectionDialog::DummyPolicy>(
               selectedCameras, d->context->mainWindowWidget());
}

QnVirtualCameraResourcePtr CommonObjectSearchSetup::currentResource() const
{
    if (!d->context)
        return {};

    return d->context->navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
}

QnVirtualCameraResourceSet CommonObjectSearchSetup::currentLayoutCameras() const
{
    if (!d->context)
        return {};

    QnVirtualCameraResourceSet cameras;
    if (auto workbenchLayout = d->context->workbench()->currentLayout())
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
    if (d->context)
        d->context->navigator()->clearTimelineSelection();
}

} // namespace nx::vms::client::desktop
