// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_search_synchronizer.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_navigator.h>

namespace nx::vms::client::desktop {

AbstractSearchSynchronizer::AbstractSearchSynchronizer(
    WindowContext* context,
    CommonObjectSearchSetup* commonSetup,
    QObject* parent)
    :
    base_type(parent),
    WindowContextAware(context),
    m_commonSetup(commonSetup)
{
    NX_CRITICAL(m_commonSetup);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged, this,
        [this]() { emit mediaWidgetAboutToBeChanged(mediaWidget(), QPrivateSignal()); });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        [this]()
        {
            const auto widget = navigator()->currentMediaWidget();
            m_mediaWidget = isMediaAccepted(widget) ? widget : nullptr;
            emit mediaWidgetChanged(m_mediaWidget.data(), QPrivateSignal());
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
    emit activeChanged(m_active, QPrivateSignal());
}

QnMediaResourceWidget* AbstractSearchSynchronizer::mediaWidget() const
{
    return m_mediaWidget.data();
}

bool AbstractSearchSynchronizer::isMediaAccepted(QnMediaResourceWidget* widget) const
{
    return widget != nullptr;
}

CommonObjectSearchSetup* AbstractSearchSynchronizer::commonSetup()
{
    return m_commonSetup;
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
