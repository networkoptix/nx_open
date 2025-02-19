// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_resource_widget.h"

#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/intercom/utils.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {

IntercomResourceWidget::IntercomResourceWidget(
    SystemContext* systemContext,
    WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(systemContext, windowContext, item, parent)
{
    // TODO: #dfisenko Is this call really needed here?
    updateButtonsVisibility();

    connect(item->layout(), &QnWorkbenchLayout::itemAdded, this,
        &IntercomResourceWidget::updateButtonsVisibility);
    connect(item->layout(), &QnWorkbenchLayout::itemRemoved, this,
        &IntercomResourceWidget::updateButtonsVisibility);
}

IntercomResourceWidget::~IntercomResourceWidget()
{
}

int IntercomResourceWidget::calculateButtonsVisibility() const
{
    int result = base_type::calculateButtonsVisibility();

    const auto intecomResource = QnResourceWidget::resource();

    if (nx::vms::common::isIntercomOnIntercomLayout(intecomResource, layoutResource()))
    {
        const nx::Uuid intercomToDeleteId = intecomResource->getId();

        QSet<nx::Uuid> otherIntercomLayoutItemIds; // Other intercom item copies on the layout.

        const auto intercomLayoutItems = layoutResource()->getItems();
        for (const common::LayoutItemData& intercomLayoutItem: intercomLayoutItems)
        {
            const auto itemResourceId = intercomLayoutItem.resource.id;
            if (itemResourceId == intercomToDeleteId && intercomLayoutItem.uuid != uuid())
                otherIntercomLayoutItemIds.insert(intercomLayoutItem.uuid);
        }

        // If single intercom copy is on the layout - removal is forbidden.
        if (otherIntercomLayoutItemIds.isEmpty())
            result &= ~Qn::CloseButton;
    }

    return result;
}

} // namespace nx::vms::client::desktop
