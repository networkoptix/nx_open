#include "poe_settings_reducer.h"

#include "poe_settings_table_view.h"

#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_constants.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_data_builder.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>

#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/api/data/network_block_data.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/mac_address.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace settings;
using namespace node_view;
using namespace node_view::details;

using PoweringStatus = nx::vms::api::NetworkPortState::PoweringStatus;
using NetworkPortState = nx::vms::api::NetworkPortState;

static const QVariant kTextAlign = static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

QString getSpeed(const NetworkPortState& port)
{
    return port.linkSpeedMbps > 0 ? QString("%1 Mbps").arg(port.linkSpeedMbps) : "-";
}

QString consumptionValue(double value)
{
    return QString::number(value, 'f', 1);
}

QString getConsumptionText(const NetworkPortState& port)
{
    if (qFuzzyIsNull(port.devicePowerConsumptionWatts) || port.devicePowerConsumptionWatts < 0)
        return "0 W";

    return QString("%1 W / %2 W")
        .arg(consumptionValue(port.devicePowerConsumptionWatts))
        .arg(consumptionValue(port.devicePowerConsumptionLimitWatts));
}

ViewNodeData poweringStatusData(const NetworkPortState& port)
{
    ViewNodeDataBuilder builder;
    switch(port.poweringStatus)
    {
        case PoweringStatus::disconnected:
            builder.withText(PoESettingsColumn::status, PoESettingsTableView::tr("Disconnected"));
            builder.withData(
                PoESettingsColumn::status, Qt::TextColorRole, colorTheme()->color("dark13"));
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

ViewNodeData wrongResourceNodeData(const QString macAddress)
{
    static const QString kEmptyText =
        PoESettingsTableView::tr("Empty", "In meaning 'There is no camera physically connected now'");
    static const QString kUnknownDeviceText =
        PoESettingsTableView::tr("< Unknown device %1 >", "In meaning 'Unknown device', %1 is system info");

    static const auto kTransparentIcon =
        []()
        {
            QPixmap pixmap(QSize(24, 24));
            pixmap.fill(Qt::transparent);
            return QIcon(pixmap);
        }();

    const bool isUnknownDevice = !nx::utils::MacAddress(macAddress).isNull();
    const auto extraText = isUnknownDevice
        ? kUnknownDeviceText.arg(macAddress)
        : kEmptyText;
    ViewNodeDataBuilder builder;

    builder
        .withData(PoESettingsColumn::camera, resourceExtraTextRole, extraText)
        .withData(PoESettingsColumn::camera, resourceColumnRole, true)
        .withData(PoESettingsColumn::camera, useItalicFontRole, true)
        .withIcon(PoESettingsColumn::camera, kTransparentIcon).data();

    if (isUnknownDevice)
        builder.withData(PoESettingsColumn::camera, Qt::TextColorRole, colorTheme()->color("light10"));
    return builder;
}

double progressValue(const NetworkPortState& port)
{
    if (!qFuzzyIsNull(port.devicePowerConsumptionLimitWatts) &&
        port.devicePowerConsumptionWatts < port.devicePowerConsumptionLimitWatts)
    {
        return 100 * port.devicePowerConsumptionWatts / port.devicePowerConsumptionLimitWatts;
    }
    return 0;

}
ViewNodeData dataFromPort(
    const NetworkPortState& port,
    QnResourcePool* resourcePool)
{
    const auto resource = resourcePool->getResourceById(port.deviceId);
    const auto resourceNodeViewData = resource
        ? getResourceNodeData(resource, PoESettingsColumn::camera, QnResourceDisplayInfo(resource).extraInfo())
        : wrongResourceNodeData(port.macAddress);

    return ViewNodeDataBuilder(resourceNodeViewData)
        .withText(PoESettingsColumn::port, QString::number(port.portNumber))

        .withText(PoESettingsColumn::consumption, getConsumptionText(port))
        .withData(PoESettingsColumn::consumption, Qt::TextAlignmentRole, kTextAlign)

        .withText(PoESettingsColumn::speed, getSpeed(port))
        .withNodeData(poweringStatusData(port))

        .withCheckedState(PoESettingsColumn::power, getPowerStatusCheckedState(port))
        .withData(PoESettingsColumn::power, useSwitchStyleForCheckboxRole, true)

        .withData(PoESettingsColumn::consumption, progressRole, progressValue(port))
        .withProperty(PoESettingsReducer::kPortNumberProperty, port.portNumber).data();
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
    // It is supposed that ports are always ordered and their count is never changed.

    if (!state.rootNode)
        return NodeViewStatePatch::fromRootNode(createPortNodes(data, resourcePool));

    NodeViewStatePatch result;

    if (data.portStates.empty())
        return NodeViewStatePatch::clearNodeViewPatch();

    int index = 0;
    for (const auto node: state.rootNode->children())
    {
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
    const double consumption =
        [data]()
        {
            double total = 0;
            for (const auto port: data.portStates)
                total += port.devicePowerConsumptionWatts;
            return total;
        }();

    const auto text = QString("%1 W / %2 W")
        .arg(consumptionValue(consumption))
        .arg(consumptionValue(data.upperPowerLimitWatts));

    const auto color = consumption > data.upperPowerLimitWatts
        ? colorTheme()->color("red_l2")
        : colorTheme()->color("light4");

    const auto nodeData = ViewNodeDataBuilder()
        .withText(PoESettingsColumn::consumption, text)
        .withData(PoESettingsColumn::consumption, Qt::TextAlignmentRole, kTextAlign)
        .withData(PoESettingsColumn::consumption, Qt::TextColorRole, color)
        .withProperty(hoverableProperty, false)
        .data();

    if (!state.rootNode)
    {
        const auto root = ViewNode::create();
        root->addChild(ViewNode::create(nodeData));
        return NodeViewStatePatch::fromRootNode(root);
    }

    if (data.portStates.empty())
        return NodeViewStatePatch::clearNodeViewPatch();

    NodeViewStatePatch result;
    const auto child = state.rootNode->children().first();
    const auto difference = child->data().difference(nodeData);
    result.appendPatchStep({child->path(), difference.updateOperation});
    return result;
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

PoESettingsStatePatch::BoolOptional PoESettingsReducer::showPreloaderChanges(
    const PoESettingsState& state,
        const bool value)
{
    return value == state.showPreloader ? PoESettingsStatePatch::BoolOptional() : value;
}

PoESettingsStatePatch::BoolOptional PoESettingsReducer::autoUpdatesChanges(
    const PoESettingsState& state,
    const bool value)
{
    return value == state.autoUpdates ? PoESettingsStatePatch::BoolOptional() : value;
}

} // namespace settings
} // namespace nx::vms::client::desktop
