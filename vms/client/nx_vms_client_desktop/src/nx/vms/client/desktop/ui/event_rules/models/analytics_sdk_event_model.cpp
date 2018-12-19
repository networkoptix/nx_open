#include "analytics_sdk_event_model.h"

#include <client/client_runtime_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/vms/api/analytics/manifest_items.h>

#include <nx/analytics/descriptor_manager.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <common/common_module.h>

namespace nx::vms::client::desktop {
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
        const QString& name,
        const QString& pluginId,
        const QString& id)
    {
        auto item = new QStandardItem(name);
        item->setData(qVariantFromValue(id), EventTypeIdRole);
        item->setData(qVariantFromValue(pluginId), DriverIdRole);
        if (parent)
            parent->appendRow(item);
        else
            appendRow(item);
        return item;
    };

    using namespace nx::vms::event;
    clear();

    nx::analytics::DescriptorManager helper(commonModule());

    // TODO: #dmishin show all 'potentially' supported event types. Return to this code when
    // `isCompatible` method is added to the SDK IEngine interface.
    const auto eventTypeDescriptors = helper.supportedEventTypesIntersection(cameras);
    const auto engineDescriptors = helper.parentEventTypeEngines(eventTypeDescriptors);
    const auto groupDescriptors = helper.parentEventTypeGroups(eventTypeDescriptors);

    const bool useEngineName = engineDescriptors.size() > 1;
    struct EngineNode
    {
        QStandardItem* item = nullptr;
        QMap<QString, QStandardItem*> groups;
    };

    EngineNode defaultPluginNode;
    QMap<QnUuid, EngineNode> items;
    for (const auto& [eventTypeId, eventTypeDescriptor]: eventTypeDescriptors)
    {
        for (const auto& scope: eventTypeDescriptor.scopes)
        {
            QStandardItem* parentItem = nullptr;
            const auto& engineId = scope.engineId;
            const auto& groupId = scope.groupId;
            if (useEngineName && !items.contains(engineId))
            {
                auto itr = engineDescriptors.find(engineId);
                if (itr == engineDescriptors.cend())
                    continue;

                const auto& engineDescriptor = itr->second;
                auto item = addItem(
                    nullptr,
                    engineDescriptor.name,
                    engineDescriptor.id.toString(),
                    QString());

                item->setFlags(item->flags().setFlag(Qt::ItemIsEnabled, false));
                items.insert(engineId, {item, {}});
            }

            if (items.contains(engineId))
                parentItem = items.value(engineId).item;

            auto& engineNode = useEngineName ? items[engineId] : defaultPluginNode;
            if (!groupId.isEmpty() && !engineNode.groups.contains(groupId))
            {
                auto itr = groupDescriptors.find(groupId);
                if (itr == groupDescriptors.cend())
                    continue;

                const auto& groupDescriptor = itr->second;
                auto item = addItem(
                    engineNode.item,
                    groupDescriptor.name,
                    engineId.toString(),
                    groupDescriptor.id);

                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                engineNode.groups.insert(groupId, item);
                parentItem = item;
            }

            if (!groupId.isEmpty() && engineNode.groups.contains(groupId))
                parentItem = engineNode.groups.value(groupId);

            addItem(
                parentItem,
                eventTypeDescriptor.name,
                engineId.toString(),
                eventTypeDescriptor.id);
        }
    }

    endResetModel();
}

bool AnalyticsSdkEventModel::isValid() const
{
    return rowCount() > 0;
}

} // namespace ui
} // namespace nx::vms::client::desktop
