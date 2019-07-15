#include "motion_search_synchronizer.h"

#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/event_search/widgets/simple_motion_search_widget.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/utils/video_cache.h>

namespace nx::vms::client::desktop {

MotionSearchSynchronizer::MotionSearchSynchronizer(
    QnWorkbenchContext* context,
    SimpleMotionSearchWidget* motionSearchWidget,
    QObject* parent)
    :
    AbstractSearchSynchronizer(context, parent),
    m_motionSearchWidget(motionSearchWidget)
{
    NX_CRITICAL(m_motionSearchWidget);

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
                ? ui::action::StartSmartSearchAction
                : ui::action::StopSmartSearchAction;

            menu()->triggerIfPossible(action, display()->widgets());
        };

    connect(this, &AbstractSearchSynchronizer::activeChanged, this, updateActiveState);

    connect(workbench(), &QnWorkbench::layoutChangeProcessStarted, this,
        [this]() { m_layoutChanging = true; });

    connect(workbench(), &QnWorkbench::layoutChangeProcessFinished, this,
        [this, updateActiveState]()
        {
            m_layoutChanging = false;
            updateActiveState(active());
        });

    connect(m_motionSearchWidget.data(), &SimpleMotionSearchWidget::cameraSetChanged,
        this, &MotionSearchSynchronizer::updateCachedDevices);

    connect(m_motionSearchWidget.data(), &SimpleMotionSearchWidget::filterRegionsChanged, this,
        [this](const QList<QRegion>& value)
        {
            const auto mediaWidget = this->mediaWidget();
            if (mediaWidget && mediaWidget->isMotionSearchModeEnabled())
                mediaWidget->setMotionSelection(value);
        });
}

void MotionSearchSynchronizer::updateAreaSelection()
{
    const auto mediaWidget = this->mediaWidget();
    if (mediaWidget && m_motionSearchWidget && active())
        m_motionSearchWidget->setFilterRegions(mediaWidget->motionSelection());
}

void MotionSearchSynchronizer::updateCachedDevices()
{
    if (!ini().cacheLiveVideoForRightPanelPreviews)
        return;

    if (auto cache = qnClientModule->videoCache())
    {
        QnUuidSet cachedDevices;
        if (active())
        {
            for (const auto& camera: m_motionSearchWidget->cameras())
                cachedDevices.insert(camera->getId());
        }

        cache->setCachedDevices(intptr_t(this), cachedDevices);
    }
}

bool MotionSearchSynchronizer::isMediaAccepted(QnMediaResourceWidget* widget) const
{
    return widget && widget->resource() && widget->resource()->toResource()->hasFlags(Qn::motion);
}

} // namespace nx::vms::client::desktop
