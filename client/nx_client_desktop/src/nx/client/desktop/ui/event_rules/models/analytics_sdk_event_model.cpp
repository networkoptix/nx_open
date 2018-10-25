#include "analytics_sdk_event_model.h"

#include <client/client_runtime_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/vms/api/analytics/manifest_items.h>

#include <nx/analytics/descriptor_list_manager.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <common/common_module.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

namespace analytics_api = nx::vms::api::analytics;

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

    const auto descriptorListManager = commonModule()->analyticsDescriptorListManager();
    clear();

    const auto deviceEventTypes = descriptorListManager->deviceDescriptors<
        analytics_api::EventTypeDescriptor>(cameras);

    const auto plugins = descriptorListManager->allDescriptorsInTheSystem<
        analytics_api::PluginDescriptor>();

    const auto groups = descriptorListManager->allDescriptorsInTheSystem<
        analytics_api::GroupDescriptor>();

#if 0
    cameras.empty()
        ? helper.systemCameraIndependentAnalyticsEvents()
        : helper.supportedAnalyticsEvents(cameras);
#endif

    const bool usePluginName = descriptorListManager->pluginIds(deviceEventTypes).size() > 1;
    struct PluginNode
    {
        QStandardItem* item;
        QMap<QString, QStandardItem*> groups;
    };

    PluginNode defaultPluginNode;
    QMap<QString, PluginNode> items;
    for (const auto& entry: deviceEventTypes)
    {
        const auto& descriptor = entry.second;
        for (const auto& path: descriptor.paths)
        {
            QStandardItem* parentItem = nullptr;
            const auto& pluginId = path.pluginId;
            const auto& groupId = path.groupId;
            if (usePluginName && !items.contains(pluginId))
            {
                auto itr = plugins.find(pluginId);
                if (itr == plugins.cend())
                    continue;

                const auto& pluginDescriptor = itr->second;
                auto item = addItem(
                    nullptr,
                    pluginDescriptor.name,
                    pluginDescriptor.id,
                    QString());

                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                items.insert(pluginId, {item, {}});
                parentItem = item;
            }

            auto& pluginNode = usePluginName ? items[pluginId] : defaultPluginNode;
            if (!groupId.isEmpty() && !pluginNode.groups.contains(groupId))
            {
                auto itr = groups.find(groupId);
                if (itr == groups.cend())
                    continue;

                const auto& groupDescriptor = itr->second;
                auto item = addItem(
                    pluginNode.item,
                    groupDescriptor.name,
                    pluginId,
                    groupDescriptor.id);

                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                pluginNode.groups.insert(groupId, item);
                parentItem = item;
            }

            addItem(parentItem, descriptor.item.name.value, pluginId, descriptor.getId());
        }
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
