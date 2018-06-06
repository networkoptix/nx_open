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
    QnCommonModuleAware(parent),
    QStandardItemModel(parent)
{
}

AnalyticsSdkEventModel::~AnalyticsSdkEventModel()
{
}

void AnalyticsSdkEventModel::loadFromCameras(const QnVirtualCameraResourceList& cameras)
{
    beginResetModel();

    auto addItem = [this](
        QStandardItem* parent,
        const nx::api::TranslatableString& name,
        const QnUuid& driverId,
        const QnUuid& id)
    {
        auto item = new QStandardItem(name.text(qnRuntime->locale()));
        item->setData(qVariantFromValue(id), EventTypeIdRole);
        item->setData(qVariantFromValue(driverId), DriverIdRole);
        if (parent)
            parent->appendRow(item);
        else
            appendRow(item);
        return item;
    };

    using namespace nx::vms::event;
    AnalyticsHelper helper(commonModule());
    clear();

    const auto items = cameras.empty()
        ? helper.systemCameraIndependentAnalyticsEvents()
        : helper.supportedAnalyticsEvents(cameras);

    const bool useDriverName = nx::vms::event::AnalyticsHelper::hasDifferentDrivers(items);

    QMap<QnUuid, QStandardItem*> groups;
    QMap<QnUuid, QStandardItem*> drivers;
    for (const auto& descriptor : items)
    {
        if (useDriverName && !drivers.contains(descriptor.driverId))
        {
            auto item = addItem(nullptr, descriptor.driverName, descriptor.driverId, QnUuid());
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            drivers.insert(descriptor.driverId, item);
        }

        QStandardItem* driver = drivers.value(descriptor.driverId);
        if (!descriptor.groupId.isNull() && !groups.contains(descriptor.groupId))
        {
            auto group = AnalyticsHelper(commonModule()).groupDescriptor(descriptor.groupId);
            auto item = addItem(driver, group.name, descriptor.driverId, descriptor.groupId);
            groups.insert(descriptor.groupId, item);
        }

        addItem(descriptor.groupId.isNull() ? driver : groups[descriptor.groupId],
            descriptor.eventName, descriptor.driverId, descriptor.eventTypeId);
    }

    endResetModel();
}

bool AnalyticsSdkEventModel::isValid() const
{
    return rowCount() > 0;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
