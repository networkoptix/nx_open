#include "motion_search_synchronizer.h"

#include <core/resource/media_resource.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/event_search/widgets/simple_motion_search_widget.h>

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

    connect(this, &AbstractSearchSynchronizer::mediaWidgetAboutToBeChanged, this,
        [this](QnMediaResourceWidget* mediaWidget)
        {
            if (!mediaWidget)
                return;

            mediaWidget->disconnect(this);
            mediaWidget->setMotionSearchModeEnabled(false);
        });

    connect(this, &AbstractSearchSynchronizer::mediaWidgetChanged, this,
        [this](QnMediaResourceWidget* mediaWidget)
        {
            updateAreaSelection();
            if (!mediaWidget)
                return;

            mediaWidget->setMotionSearchModeEnabled(active());

            connect(mediaWidget, &QnMediaResourceWidget::motionSearchModeEnabled,
                this, &AbstractSearchSynchronizer::setActive);

            connect(mediaWidget, &QnMediaResourceWidget::motionSelectionChanged,
                this, &MotionSearchSynchronizer::updateAreaSelection);
        });

    connect(this, &AbstractSearchSynchronizer::activeChanged, this,
        [this](bool isActive)
        {
            updateAreaSelection();
            setTimeContentDisplayed(Qn::MotionContent, isActive);

            if (auto mediaWidget = this->mediaWidget())
                mediaWidget->setMotionSearchModeEnabled(isActive);
        });

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

bool MotionSearchSynchronizer::isMediaAccepted(QnMediaResourceWidget* widget) const
{
    return widget && widget->resource() && widget->resource()->toResource()->hasFlags(Qn::motion);
}

} // namespace nx::vms::client::desktop
