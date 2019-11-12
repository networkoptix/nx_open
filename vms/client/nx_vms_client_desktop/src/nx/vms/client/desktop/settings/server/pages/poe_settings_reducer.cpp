#include "poe_settings_reducer.h"

#include "poe_settings_table_view.h"

#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_constants.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_data_builder.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/api/data/network_block_data.h>
#include <nx/utils/math/fuzzy.h>


namespace {

using namespace nx::vms::client::desktop;
using namespace settings;
using namespace node_view;
using namespace node_view::details;

using PoweringStatus = nx::vms::api::NetworkPortState::PoweringStatus;
using NetworkPortState = nx::vms::api::NetworkPortState;

static const QVariant kTextAlign = static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
static const QString kEmptyText =
    PoESettingsTableView::tr("Empty", "In meaning 'There is no camera physically connected now'");

QString getSpeed(const NetworkPortState& port)
{
    return port.linkSpeedMbps > 0 ? QString("%1 Mbps").arg(port.linkSpeedMbps) : "-";
}

QString getConsumptionText(const NetworkPortState& port)
{
    if (qFuzzyIsNull(port.devicePowerConsumptionWatts) || port.devicePowerConsumptionWatts < 0)
        return "0 W";

    return QString("%1 W / %2 W")
        .arg(port.devicePowerConsumptionWatts)
        .arg(port.devicePowerConsumptionLimitWatts);
}

// TODO: fix colors
ViewNodeData poweringStatusData(const NetworkPortState& port)
{
    ViewNodeDataBuilder builder;
    switch(port.poweringStatus)
    {
        case PoweringStatus::disconnected:
            builder.withText(PoESettingsColumn::status, PoESettingsTableView::tr("Disconnected"));
            builder.withData(
                PoESettingsColumn::status, Qt::TextColorRole, colorTheme()->color("green_core"));
            break;
        case PoweringStatus::connected:
            builder.withText(PoESettingsColumn::status, PoESettingsTableView::tr("Connected"));
            break;
        case PoweringStatus::powered:
            builder.withText(PoESettingsColumn::status, PoESettingsTableView::tr("Powered"));
            builder.withData(PoESettingsColumn::status, Qt::TextColorRole,
                colorTheme()->color("green_core"));
            break;
        default:
            NX_ASSERT(false, "Unexpected network port powering status!");
            builder.withText(PoESettingsColumn::status, PoESettingsTableView::tr("Unexpected"));
            break;
    }
    return builder.data();
}

Qt::CheckState getPowerStatusCheckedState(const NetworkPortState& port)
{
    return port.poweringMode == NetworkPortState::PoweringMode::off
        ? Qt::Unchecked
        : Qt::Checked;
}

ViewNodeData dataFromPort(
    const NetworkPortState& port,
    QnResourcePool* resourcePool)
{
    const auto resource = resourcePool->getResourceById(port.deviceId);
    const auto resourceNodeViewData =
        getResourceNodeData(resource, PoESettingsColumn::camera, kEmptyText);

    return ViewNodeDataBuilder(resourceNodeViewData)
        .withText(PoESettingsColumn::port, QString::number(port.portNumber))

        .withText(PoESettingsColumn::consumption, getConsumptionText(port))
        .withData(PoESettingsColumn::consumption, Qt::TextAlignmentRole, kTextAlign)
        .withData(PoESettingsColumn::consumption, progressRole,
            100 * port.devicePowerConsumptionWatts / port.devicePowerConsumptionLimitWatts)

        .withText(PoESettingsColumn::speed, getSpeed(port))
        .withNodeData(poweringStatusData(port))

        .withCheckedState(PoESettingsColumn::power, getPowerStatusCheckedState(port))
        .withData(PoESettingsColumn::power, useSwitchStyleForCheckboxRole, true)

        .withProperty(PoESettingsReducer::kPortNumberProperty, port.portNumber)
        .data();
}

NodePtr createPortNodes(
    const nx::vms::api::NetworkBlockData& data,
    QnResourcePool* resourcePool)
{
    if (data.portStates.empty())
        return NodePtr();

    const auto root = ViewNode::create();
    for (const auto& port: data.portStates)
        root->addChild(ViewNode::create(dataFromPort(port, resourcePool)));

    return root;
}

} // namespace

namespace nx::vms::client::desktop {
namespace settings {

using namespace node_view;
using namespace node_view::details;

NodeViewStatePatch PoESettingsReducer::blockDataChangesPatch(
    const NodeViewState& state,
    const nx::vms::api::NetworkBlockData& data,
    QnResourcePool* resourcePool)
{
    // TODO: check for server switch.

    // It is supposed that ports are always ordered and their count is never changed.

    if (!state.rootNode)
        return NodeViewStatePatch::fromRootNode(createPortNodes(data, resourcePool));

    NodeViewStatePatch result;

    int index = 0;
    for (const auto node: state.rootNode->children())
    {
        ViewNodeData forRemove;
        ViewNodeData forOverride;
        const auto source = node->data();
        const auto target = dataFromPort(data.portStates[index++], resourcePool);

        const auto difference = source.difference(target);
        result.appendPatchStep({node->path(), difference.updateOperation});
    }
    return result;
}

node_view::details::NodeViewStatePatch PoESettingsReducer::totalsDataChangesPatch(
    const node_view::details::NodeViewState& state,
    const nx::vms::api::NetworkBlockData& data)
{
    NodeViewStatePatch patch;
    return patch;
}

PoESettingsStatePatch::BoolOptional PoESettingsReducer::poeOverBudgetChanges(
    const PoESettingsState& state,
    const nx::vms::api::NetworkBlockData& data)
{
    return data.isInPoeOverBudgetMode == state.showPoEOverBudgetWarning
        ? PoESettingsStatePatch::BoolOptional()
        : data.isInPoeOverBudgetMode;
}

PoESettingsStatePatch::BoolOptional PoESettingsReducer::blockUiChanges(
    const PoESettingsState& state,
    const bool blockUi)
{
    return blockUi == state.blockUi ? PoESettingsStatePatch::BoolOptional() : blockUi;
}

} // namespace settings
} // namespace nx::vms::client::desktop
