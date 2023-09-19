// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "special_layout.h"

#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/workbench/panels/special_layout_panel_widget.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

SpecialLayout::SpecialLayout(WindowContext* windowContext, const LayoutResourcePtr& resource):
    base_type(resource),
    m_panelWidget(new SpecialLayoutPanelWidget(windowContext, resource, this))
{
    setFlags(flags() | QnLayoutFlag::SpecialBackground);
}

SpecialLayout::~SpecialLayout()
{
}

void SpecialLayout::setPanelWidget(QGraphicsWidget* widget)
{
    if (m_panelWidget == widget)
        return;

    if (m_panelWidget)
        delete m_panelWidget;

    m_panelWidget = widget;
    if (m_panelWidget)
        m_panelWidget->setParent(this);

    emit panelWidgetChanged();
}

QGraphicsWidget* SpecialLayout::panelWidget() const
{
    return m_panelWidget;
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
