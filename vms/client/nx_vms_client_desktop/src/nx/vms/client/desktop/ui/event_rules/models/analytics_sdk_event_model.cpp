// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_sdk_event_model.h"

#include <client/client_runtime_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/analytics/analytics_entities_tree.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {
namespace ui {

AnalyticsSdkEventModel::AnalyticsSdkEventModel(
    SystemContext* systemContext,
    QObject* parent)
    :
    QStandardItemModel(parent),
    SystemContextAware(systemContext)
{
}

AnalyticsSdkEventModel::~AnalyticsSdkEventModel()
{
}

void AnalyticsSdkEventModel::loadFromCameras(
    const QnVirtualCameraResourceList& cameras,
    const nx::vms::event::EventParameters& currentEventParameters)
{
    loadFromCameras(
        cameras,
        currentEventParameters.getAnalyticsEngineId(),
        currentEventParameters.getAnalyticsEventTypeId());
}

void AnalyticsSdkEventModel::loadFromCameras(
        const QnVirtualCameraResourceList& cameras,
        QnUuid engineId,
        QString eventTypeId)
{
    auto addItem =
        [this](QStandardItem* parent, core::AnalyticsEntitiesTreeBuilder::NodePtr node)
        {
            const bool isValidEventType = !node->entityId.isEmpty();

            auto item = new QStandardItem(node->text);
            item->setData(QVariant::fromValue(node->entityId), EventTypeIdRole);
            item->setData(QVariant::fromValue(node->engineId), EngineIdRole);
            item->setData(QVariant::fromValue(isValidEventType), ValidEventRole);
            item->setSelectable(isValidEventType);

            if (parent)
                parent->appendRow(item);
            else
                appendRow(item);
            return item;
        };

    auto addItemRecursive = nx::utils::y_combinator(
        [addItem](auto addItemRecursive, QStandardItem* parent, auto root) -> void
        {
            for (auto node: root->children)
            {
                const auto menuItem = addItem(parent, node);
                addItemRecursive(menuItem, node);
            }
        });


    const auto root = core::AnalyticsEntitiesTreeBuilder::eventTypesForRulesPurposes(
        systemContext(),
        cameras,
        {
            {
                engineId,
                eventTypeId
            }
        });

    beginResetModel();
    clear();
    addItemRecursive(/*parent*/ nullptr, root);
    endResetModel();
}

bool AnalyticsSdkEventModel::isValid() const
{
    const auto items = match(
        index(0, 0),
        ValidEventRole,
        /*value*/ QVariant::fromValue(true),
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    return items.size() > 0;
}

} // namespace ui
} // namespace nx::vms::client::desktop
