#include "analytics_sdk_event_model.h"

#include <client/client_runtime_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

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
            const bool useDriverName = m_usedDrivers.size() > 0;
            return useDriverName
                ? lit("%1 - %2").arg(m_usedDrivers.value(item.driverId)).arg(item.eventName)
                : item.eventName;
        }

        case EventTypeIdRole:
            return qVariantFromValue(item.eventTypeId);

        default:
            break;
    }

    return QVariant();
}

void AnalyticsSdkEventModel::loadFromCameras(const QnVirtualCameraResourceList& cameras)
{
    /*
    * Notes:
    * 1) Event type id is not unique between different drivers.
    * 2) Different servers can have different versions of the same driver with different event
    * types list. This situation is not handled right now. // TODO: #GDM #analytics should we???
    */

    beginResetModel();
    m_items.clear();
    m_usedDrivers.clear();

    for (const auto& camera: cameras)
    {
        const auto driverId = camera->analyticsDriverId();
        if (driverId.isNull() || m_usedDrivers.contains(driverId))
            continue;

        const auto server = camera->getParentServer();
        NX_EXPECT(server);
        if (!server)
            continue;

        const auto drivers = server->analyticsDrivers();
        const auto driver = std::find_if(drivers.cbegin(), drivers.cend(),
            [driverId](const nx::api::AnalyticsDriverManifest& manifest)
            {
                return manifest.driverId == driverId;
            });
        NX_EXPECT(driver != drivers.cend());
        if (driver == drivers.cend())
            continue;

        m_usedDrivers[driverId] = driver->driverName.text(qnRuntime->locale());
        for (const auto eventType: driver->outputEventTypes)
        {
            Item item;
            item.driverId = driverId;
            item.eventTypeId = eventType.eventTypeId;
            item.eventName = eventType.eventName.text(qnRuntime->locale());
            m_items.push_back(item);
        }
    }

    m_valid = !m_items.empty();
    if (!m_valid)
    {
        Item dummy;
        dummy.eventName = tr("No event types supported");
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
