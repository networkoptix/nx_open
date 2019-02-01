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
        const QnUuid& engineId,
        const QString& id)
    {
        auto item = new QStandardItem(name);
        item->setData(qVariantFromValue(id), EventTypeIdRole);
        item->setData(qVariantFromValue(engineId), DriverIdRole);
        if (parent)
            parent->appendRow(item);
        else
            appendRow(item);
        return item;
    };

    using namespace nx::vms::event;
    clear();

    nx::analytics::EventTypeDescriptorManager eventTypeDescriptorManager(commonModule());
    nx::analytics::EngineDescriptorManager engineDescriptorManager(commonModule());
    nx::analytics::GroupDescriptorManager groupDescriptorManager(commonModule());

    const auto scopedEventTypeIds = eventTypeDescriptorManager
        .compatibleEventTypeIdsIntersection(cameras);

    for (const auto& [engineId, eventTypeIdsByGroup]: scopedEventTypeIds)
    {
        const auto engineDescriptor = engineDescriptorManager.descriptor(engineId);
        QStandardItem* parentItem = nullptr;

        if (!engineDescriptor)
            continue;

        parentItem = addItem(
            nullptr,
            engineDescriptor->name,
            engineId,
            QString());

        for (const auto& [groupId, eventTypeIds]: eventTypeIdsByGroup)
        {
            const auto groupDescriptor = groupDescriptorManager.descriptor(groupId);
            if (groupDescriptor)
            {
                parentItem = addItem(
                    parentItem,
                    groupDescriptor->name,
                    engineId,
                    groupDescriptor->id);
            }

            for (const auto& eventTypeId: eventTypeIds)
            {
                const auto eventTypeDescriptor =
                    eventTypeDescriptorManager.descriptor(eventTypeId);

                if (!eventTypeDescriptor)
                    continue;

                const bool isHidden = eventTypeDescriptor->flags.testFlag(
                    nx::vms::api::analytics::EventTypeFlag::hidden);

                if (isHidden)
                    continue;

                addItem(
                    parentItem,
                    eventTypeDescriptor->name,
                    engineId,
                    eventTypeDescriptor->id);
            }
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
