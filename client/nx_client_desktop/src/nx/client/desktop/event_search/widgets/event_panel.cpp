#include "event_panel.h"
#include "private/event_panel_p.h"

namespace nx {
namespace client {
namespace desktop {

EventPanel::EventPanel(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
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

} // namespace desktop
} // namespace client
} // namespace nx
