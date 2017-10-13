#include "event_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

EventListModel::Private::Private(EventListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q)
{
}

EventListModel::Private::~Private()
{
}

} // namespace
} // namespace client
} // namespace nx
