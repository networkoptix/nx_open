#include "event_panel.h"
#include "private/event_panel_p.h"

namespace nx {
namespace client {
namespace desktop {

EventPanel::EventPanel(QWidget* parent): EventPanel(nullptr, parent)
{
}

EventPanel::EventPanel(QnWorkbenchContext* context, QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    d(new Private(this))
{
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
