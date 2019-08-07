#include "analytics_sdk_event_model.h"

#include <client/client_runtime_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/vms/client/desktop/analytics/analytics_events_tree.h>

#include <common/common_module.h>

#include <nx/utils/std/algorithm.h>

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
    auto addItem =
        [this](QStandardItem* parent, AnalyticsEventsTreeBuilder::NodePtr node)
        {
            const bool isValidEventType = !node->eventTypeId.isEmpty();

            auto item = new QStandardItem(node->text);
            item->setData(qVariantFromValue(node->eventTypeId), EventTypeIdRole);
            item->setData(qVariantFromValue(node->engineId), EngineIdRole);
            item->setData(qVariantFromValue(isValidEventType), ValidEventRole);
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

    AnalyticsEventsTreeBuilder eventsTreeBuilder(commonModule());
    const auto root = eventsTreeBuilder.eventTypesForRulesPurposes(cameras);

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
        /*value*/ qVariantFromValue(true),
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    return items.size() > 0;
}

} // namespace ui
} // namespace nx::vms::client::desktop
