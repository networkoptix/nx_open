#include "analytics_search_list_model.h"
#include "private/analytics_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

AnalyticsSearchListModel::AnalyticsSearchListModel(QObject* parent):
    base_type([this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(d_func()))
{
}

QRectF AnalyticsSearchListModel::filterRect() const
{
    return d->filterRect();
}

void AnalyticsSearchListModel::setFilterRect(const QRectF& relativeRect)
{
    d->setFilterRect(relativeRect);
}

bool AnalyticsSearchListModel::setData(const QModelIndex& index, const QVariant& /*value*/, int role)
{
    if (!index.isValid() || index.model() != this)
        return false;

    switch (role)
    {
        case Qn::DefaultNotificationRole:
            return d->defaultAction(index.row());

        default:
            return false;
    }
}

} // namespace
} // namespace client
} // namespace nx
