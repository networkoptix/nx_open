#include "event_list_model.h"
#include "private/event_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

EventListModel::EventListModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
}

EventListModel::~EventListModel()
{
}

int EventListModel::rowCount(const QModelIndex& parent) const
{
    // TODO: #vkutin
    return 0;
}

QVariant EventListModel::data(const QModelIndex& index, int role) const
{
    // TODO: #vkutin
    return QVariant();
}

void EventListModel::clear()
{
    ScopedReset reset(this);

    // TODO: #vkutin
}

} // namespace
} // namespace client
} // namespace nx
