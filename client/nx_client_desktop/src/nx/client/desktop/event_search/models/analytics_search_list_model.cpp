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

QString AnalyticsSearchListModel::filterText() const
{
    return d->filterText();
}

void AnalyticsSearchListModel::setFilterText(const QString& value)
{
    d->setFilterText(value);
}

bool AnalyticsSearchListModel::isConstrained() const
{
    return filterRect().isValid() || !filterText().isEmpty() || base_type::isConstrained();
}

} // namespace desktop
} // namespace client
} // namespace nx
