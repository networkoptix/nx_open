#include "abstract_search_synchronizer.h"

#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

AbstractSearchSynchronizer::AbstractSearchSynchronizer(
    QnWorkbenchContext* context,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(context)
{
    NX_CRITICAL(this->context());

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged, this,
        [this]() { emit mediaWidgetAboutToBeChanged(mediaWidget(), {}); });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        [this]()
        {
            const auto widget = navigator()->currentMediaWidget();
            m_mediaWidget = isMediaAccepted(widget) ? widget : nullptr;
            emit mediaWidgetChanged(m_mediaWidget.data(), {});
        });
}

bool AbstractSearchSynchronizer::active() const
{
    return m_active;
}

void AbstractSearchSynchronizer::setActive(bool value)
{
    if (m_active == value)
        return;

    m_active = value;
    emit activeChanged(m_active, {});
}

QnMediaResourceWidget* AbstractSearchSynchronizer::mediaWidget() const
{
    return m_mediaWidget.data();
}

bool AbstractSearchSynchronizer::isMediaAccepted(QnMediaResourceWidget* widget) const
{
    return widget != nullptr;
}

void AbstractSearchSynchronizer::setTimeContentDisplayed(
    Qn::TimePeriodContent content, bool displayed)
{
    NX_ASSERT(content != Qn::RecordingContent);
    if (content == Qn::RecordingContent)
        return;

    if (displayed)
    {
        navigator()->setSelectedExtraContent(content);
    }
    else
    {
        if (navigator()->selectedExtraContent() == content)
            navigator()->setSelectedExtraContent(Qn::RecordingContent /*means none*/);
    }
}

} // namespace nx::vms::client::desktop
