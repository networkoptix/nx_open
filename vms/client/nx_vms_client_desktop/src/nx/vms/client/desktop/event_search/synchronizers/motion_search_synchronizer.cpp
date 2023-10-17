// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_search_synchronizer.h"

#include <core/resource/camera_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/video_cache.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_display.h>

using nx::vms::client::core::MotionSelection;

namespace nx::vms::client::desktop {

MotionSearchSynchronizer::MotionSearchSynchronizer(
    WindowContext* context,
    CommonObjectSearchSetup* commonSetup,
    SimpleMotionSearchListModel* model,
    QObject* parent)
    :
    AbstractSearchSynchronizer(context, parent),
    m_commonSetup(commonSetup),
    m_model(model)
{
    NX_CRITICAL(m_commonSetup && m_model);

    const auto connectToDisplayWidget =
        [this](QnResourceWidget* widget)
        {
            if (!isMediaAccepted(qobject_cast<QnMediaResourceWidget*>(widget)))
                return;

            widget->setOption(QnResourceWidget::DisplayMotion, active());

            connect(widget, &QnResourceWidget::optionsChanged, this,
                [this, widget](QnResourceWidget::Options changedOptions)
                {
                    if (!m_layoutChanging && changedOptions.testFlag(QnResourceWidget::DisplayMotion))
                        setActive(widget->options().testFlag(QnResourceWidget::DisplayMotion));
                });

            const auto mediaWidget = static_cast<QnMediaResourceWidget*>(widget);
            connect(mediaWidget, &QnMediaResourceWidget::motionSelectionChanged, this,
                [this]
                {
                    if (sender() == this->mediaWidget())
                        updateAreaSelection();
                });
        };

    for (auto widget: display()->widgets())
        connectToDisplayWidget(widget);

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this, connectToDisplayWidget);

    connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this,
        [this](QnResourceWidget* widget) { widget->disconnect(this); });

    connect(this, &AbstractSearchSynchronizer::mediaWidgetChanged,
        this, &MotionSearchSynchronizer::updateAreaSelection);

    const auto updateActiveState =
        [this](bool isActive)
        {
            updateAreaSelection();
            updateCachedDevices();
            setTimeContentDisplayed(Qn::MotionContent, isActive);

            const auto action = isActive
                ? menu::StartSmartSearchAction
                : menu::StopSmartSearchAction;

            menu()->triggerIfPossible(action, display()->widgets());
        };

    connect(this, &AbstractSearchSynchronizer::activeChanged, this, updateActiveState);

    connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        [this]() { m_layoutChanging = true; });

    connect(workbench(), &Workbench::currentLayoutChanged, this,
        [this, updateActiveState]()
        {
            m_layoutChanging = false;
            updateActiveState(active());
        });

    connect(m_commonSetup.data(), &CommonObjectSearchSetup::selectedCamerasChanged,
        this, &MotionSearchSynchronizer::updateCachedDevices);

    connect(m_model.data(), &SimpleMotionSearchListModel::filterRegionsChanged, this,
        [this]()
        {
            const auto mediaWidget = this->mediaWidget();
            if (mediaWidget && mediaWidget->isMotionSearchModeEnabled())
                mediaWidget->setMotionSelection(m_model->filterRegions());
        });
}

void MotionSearchSynchronizer::updateAreaSelection()
{
    const auto mediaWidget = this->mediaWidget();
    if (mediaWidget && m_model && active())
        m_model->setFilterRegions(mediaWidget->motionSelection());
}

void MotionSearchSynchronizer::updateCachedDevices()
{
    std::map<SystemContext*, QnUuidSet> cachedDevicesByContext;
    if (active() && m_commonSetup)
    {
        for (const auto& camera: m_commonSetup->selectedCameras())
        {
            auto systemContext = SystemContext::fromResource(camera);
            cachedDevicesByContext[systemContext].insert(camera->getId());
        }
    }
    for (const auto& [systemContext, cachedDevices]: cachedDevicesByContext)
        systemContext->videoCache()->setCachedDevices(intptr_t(this), cachedDevices);
}

bool MotionSearchSynchronizer::isMediaAccepted(QnMediaResourceWidget* widget) const
{
    return widget && widget->resource() && widget->resource()->toResource()->hasFlags(Qn::motion);
}

} // namespace nx::vms::client::desktop
