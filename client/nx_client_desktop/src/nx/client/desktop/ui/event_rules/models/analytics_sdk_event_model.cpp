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

AnalyticsSdkEventModel::AnalyticsSdkEventModel(QObject* parent):
    base_type(parent)
{
}

int AnalyticsSdkEventModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid()
        ? 0
        : m_items.size();
}

QVariant AnalyticsSdkEventModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() > m_items.size())
        return QVariant();

    const auto& item = m_items[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
        case Qt::AccessibleTextRole:
        case Qt::AccessibleDescriptionRole:
        {
            const bool useDriverName = m_valid
                && nx::vms::event::AnalyticsHelper::hasDifferentDrivers(m_items);
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
    m_items = nx::vms::event::AnalyticsHelper::analyticsEvents(cameras);
    m_valid = !m_items.empty();
    if (!m_valid)
    {
        nx::api::AnalyticsEventTypeWithRef dummy;
        dummy.eventName.value = tr("No event types supported");
        m_items.push_back(dummy);
    }
    endResetModel();
}

bool AnalyticsSdkEventModel::isValid() const
{
    return m_valid;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
