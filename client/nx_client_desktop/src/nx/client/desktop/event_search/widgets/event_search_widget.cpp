#include "event_search_widget.h"
#include "private/event_search_widget_p.h"

namespace nx {
namespace client {
namespace desktop {

EventSearchWidget::EventSearchWidget(QWidget* parent):
    QWidget(parent),
    d(new Private(this))
{
}

EventSearchWidget::~EventSearchWidget()
{
}

} // namespace
} // namespace client
} // namespace nx
