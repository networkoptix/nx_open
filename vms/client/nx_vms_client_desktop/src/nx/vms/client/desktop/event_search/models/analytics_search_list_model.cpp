#include "analytics_search_list_model.h"
#include "private/analytics_search_list_model_p.h"

#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop {

AnalyticsSearchListModel::AnalyticsSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, [this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(base_type::d.data()))
{
    setLiveSupported(true);
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

QString AnalyticsSearchListModel::selectedObjectType() const
{
    return d->selectedObjectType();
}

void AnalyticsSearchListModel::setSelectedObjectType(const QString& value)
{
    d->setSelectedObjectType(value);
}

bool AnalyticsSearchListModel::isConstrained() const
{
    return filterRect().isValid() || !filterText().isEmpty() || base_type::isConstrained();
}

bool AnalyticsSearchListModel::hasAccessRights() const
{
    return accessController()->hasGlobalPermission(GlobalPermission::viewArchive);
}

bool AnalyticsSearchListModel::isCameraApplicable(const QnVirtualCameraResourcePtr& camera) const
{
    return base_type::isCameraApplicable(camera) && d->isCameraApplicable(camera);
}

} // namespace nx::vms::client::desktop
