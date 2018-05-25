#include "event_panel.h"
#include "private/event_panel_p.h"

#include <QtGui/QMouseEvent>

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

void EventPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && event->button() == Qt::RightButton)
        event->accept();
}


} // namespace desktop
} // namespace client
} // namespace nx
