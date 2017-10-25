#include "event_panel.h"
#include "private/event_panel_p.h"

#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>

namespace nx {
namespace client {
namespace desktop {

EventPanel::EventPanel(QWidget* parent): EventPanel(nullptr, parent)
{
}

EventPanel::EventPanel(QnWorkbenchContext* context, QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    d(new Private(this))
{
    connect(this->context()->display(), &QnWorkbenchDisplay::widgetChanged, this,
        [this](Qn::ItemRole role)
        {
            if (role != Qn::CentralRole)
                return;

            const auto widget = this->context()->display()->widget(Qn::CentralRole);
            const auto camera = widget
                ? widget->resource().dynamicCast<QnVirtualCameraResource>()
                : QnVirtualCameraResourcePtr();

            d->setCamera(camera);
        });
}

EventPanel::~EventPanel()
{
}

void EventPanel::paintEvent(QPaintEvent* /*event*/)
{
    d->paintBackground();
}

} // namespace
} // namespace client
} // namespace nx
