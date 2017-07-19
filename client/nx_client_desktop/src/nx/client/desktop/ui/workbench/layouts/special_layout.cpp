#include "special_layout.h"

#include <core/resource/layout_resource.h>

#include <ui/workbench/workbench_layout.h>
#include <nx/client/desktop/ui/workbench/panels/special_layout_panel_widget.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

SpecialLayout::SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent):
    base_type(resource, parent),
    m_panelWidget(new SpecialLayoutPanelWidget(resource, this))
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
} // namespace desktop
} // namespace client
} // namespace nx

