#include "analytics_sdk_event_model.h"

#include <client/client_runtime_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/api/analytics/driver_manifest.h>
#include <nx/api/analytics/supported_events.h>

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
    m_items.clear();
    m_usedDrivers.clear();

    // Events are defined by pair of driver id and event type id.
    using ItemKey = QPair<QnUuid, QnUuid>;
    QSet<ItemKey> addedEvents;

    for (const auto& camera: cameras)
    {
        const auto supportedEvents = camera->analyticsSupportedEvents();
        for (const auto& supportedEvent: supportedEvents)
        {
            const auto driverId = supportedEvent.driverId;
            if (driverId.isNull())
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
                if (!supportedEvent.eventTypes.contains(eventType.eventTypeId))
                    continue;

                // Ignore duplicating events.
                ItemKey key(driverId, eventType.eventTypeId);
                if (addedEvents.contains(key))
                    continue;
                addedEvents.insert(key);

                Item item;
                item.driverId = driverId;
                item.eventTypeId = eventType.eventTypeId;
                item.eventName = eventType.eventName.text(qnRuntime->locale());
                m_items.push_back(item);
            }
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
