#include "analytics_sdk_event_model.h"

#include <client/client_runtime_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/api/analytics/analytics_event.h>
#include <nx/vms/event/analytics_helper.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

struct AnalyticsSdkEventModel::Private
{
    QList<nx::vms::event::AnalyticsHelper::EventDescriptor> items;
    bool valid = false;
};

AnalyticsSdkEventModel::AnalyticsSdkEventModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

AnalyticsSdkEventModel::~AnalyticsSdkEventModel()
{
}

int AnalyticsSdkEventModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid()
        ? 0
        : d->items.size();
}

QVariant AnalyticsSdkEventModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() > d->items.size())
        return QVariant();

    const auto& item = d->items[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
        {
            const bool useDriverName = d->valid
                && nx::vms::event::AnalyticsHelper::hasDifferentDrivers(d->items);
            return useDriverName
                ? lit("%1 - %2")
                    .arg(item.driverName.text(qnRuntime->locale()))
                    .arg(item.eventName.text(qnRuntime->locale()))
                : item.eventName.text(qnRuntime->locale());
        }

        case EventTypeIdRole:
            return qVariantFromValue(item.eventTypeId);

        case DriverIdRole:
            return qVariantFromValue(item.driverId);

        default:
            break;
    }

    return QVariant();
}

void AnalyticsSdkEventModel::loadFromCameras(const QnVirtualCameraResourceList& cameras)
{
    beginResetModel();
    d->items = nx::vms::event::AnalyticsHelper::supportedAnalyticsEvents(cameras);
    d->valid = !d->items.empty();
    if (!d->valid)
    {
        nx::vms::event::AnalyticsHelper::EventDescriptor dummy;
        dummy.eventName.value = tr("No event types supported");
        d->items.push_back(dummy);
    }
    endResetModel();
}

bool AnalyticsSdkEventModel::isValid() const
{
    return d->valid;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
