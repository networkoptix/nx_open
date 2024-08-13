// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_settings_reducer.h"

#include "poe_settings_table_view.h"

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helper.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_constants.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_data_builder.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>

#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/api/data/network_block_data.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/mac_address.h>
#include <nx/vms/event/strings_helper.h>

namespace {

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace node_view;
using namespace node_view::details;

using PoweringStatus = nx::vms::api::NetworkPortState::PoweringStatus;
using NetworkPortState = nx::vms::api::NetworkPortState;

static const QVariant kTextAlign = static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

QString getConsumptionText(const NetworkPortState& port)
{
    return nx::vms::event::StringsHelper::poeConsumptionString(
        port.devicePowerConsumptionWatts, port.devicePowerConsumptionLimitWatts);
}

ViewNodeData poweringStatusData(const NetworkPortState& port)
{
    ViewNodeDataBuilder builder;
    switch(port.poweringStatus)
    {
        case PoweringStatus::disconnected:
            builder.withText(PoeSettingsColumn::status, PoeSettingsTableView::tr("Disconnected"));
            builder.withData(
                PoeSettingsColumn::status, Qt::ForegroundRole, core::colorTheme()->color("dark13"));
            break;
        case PoweringStatus::connected:
            builder.withText(PoeSettingsColumn::status, PoeSettingsTableView::tr("Connected"));
            break;
        case PoweringStatus::powered:
            builder.withText(PoeSettingsColumn::status, PoeSettingsTableView::tr("Powered"));
            builder.withData(PoeSettingsColumn::status, Qt::ForegroundRole,
                core::colorTheme()->color("green"));
            break;
        default:
            NX_ASSERT(false, "Unexpected network port powering status!");
            builder.withText(PoeSettingsColumn::status, PoeSettingsTableView::tr("Unexpected"));
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

QString uknownDeviceString(const QString& macAddress)
{
    auto braced = [](const QString& source) { return "< " + source + " >"; };

    static const QString kUnknownDeviceText = PoeSettingsTableView::tr("Unknown device");
    static const QString kUnknownDeviceWithMacText =
        PoeSettingsTableView::tr("Unknown device %1", "In meaning 'Unknown device', %1 is system info");
    return nx::utils::MacAddress(macAddress).isNull()
        ? braced(kUnknownDeviceText)
        : braced(kUnknownDeviceWithMacText.arg(macAddress));
}

ViewNodeData wrongResourceNodeData(const QString& macAddress, bool connected)
{
    static const QString kEmptyText =
        PoeSettingsTableView::tr("Empty", "In meaning 'There is no camera physically connected now'");

    static const auto kTransparentIcon =
        []()
        {
            QPixmap pixmap(QSize(24, 24));
            pixmap.fill(Qt::transparent);
            return QIcon(pixmap);
        }();

    const bool isUnknownDevice = !nx::utils::MacAddress(macAddress).isNull() || connected;
    const auto extraText = isUnknownDevice ? uknownDeviceString(macAddress) : kEmptyText;

    ViewNodeDataBuilder builder;
    builder
        .withData(PoeSettingsColumn::camera, resourceExtraTextRole, extraText)
        .withData(PoeSettingsColumn::camera, resourceColumnRole, true)
        .withData(PoeSettingsColumn::camera, useItalicFontRole, true)
        .withIcon(PoeSettingsColumn::camera, kTransparentIcon).data();

    if (isUnknownDevice)
        builder.withData(PoeSettingsColumn::camera, Qt::ForegroundRole, core::colorTheme()->color("light10"));
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
    const auto resource = resourcePool->getResourceByMacAddress(port.macAddress)
        .staticCast<QnResource>();
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto resourceNodeViewData =
        [port, resource, camera]()
        {
            if (!resource)
            {
                const bool connected = port.poweringStatus != NetworkPortState::PoweringStatus::disconnected;
                return wrongResourceNodeData(port.macAddress, connected);
            }

            const auto extraInfo = QnResourceDisplayInfo(resource).extraInfo();
            return camera && !camera->getGroupId().isEmpty()
                ? getGroupNodeData(camera, PoeSettingsColumn::camera, extraInfo)
                : getResourceNodeData(resource, PoeSettingsColumn::camera, extraInfo);
        }();

    return ViewNodeDataBuilder(resourceNodeViewData)
        .withText(PoeSettingsColumn::port, QString::number(port.portNumber))

        .withText(PoeSettingsColumn::consumption, getConsumptionText(port))
        .withData(PoeSettingsColumn::consumption, Qt::TextAlignmentRole, kTextAlign)

        .withNodeData(poweringStatusData(port))

        .withCheckedState(PoeSettingsColumn::power, getPowerStatusCheckedState(port))
        .withData(PoeSettingsColumn::power, useSwitchStyleForCheckboxRole, true)

        .withData(PoeSettingsColumn::consumption, progressRole, progressValue(port))
        .withProperty(PoeSettingsReducer::kPortNumberProperty, port.portNumber).data();
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

OperationData removeUserCheckedActions(OperationData removeData)
{
    if (removeData.operation != PatchStepOperation::removeDataOperation)
        return removeData;

    auto data = removeData.data.value<RemoveData>();
    for (const auto column: data.keys())
        data[column].removeAll(ViewNodeHelper::makeUserActionRole(Qt::CheckStateRole));

    return {PatchStepOperation::removeDataOperation, QVariant::fromValue(data)};
}

} // namespace

namespace nx::vms::client::desktop {

using namespace node_view;
using namespace node_view::details;

NodeViewStatePatch PoeSettingsReducer::blockDataChangesPatch(
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
        result.appendPatchStep({node->path(), removeUserCheckedActions(difference.removeOperation)});
    }
    return result;
}

node_view::details::NodeViewStatePatch PoeSettingsReducer::totalsDataChangesPatch(
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


    const auto text = nx::vms::event::StringsHelper::poeOverallConsumptionString(
        consumption, data.upperPowerLimitWatts);

    const auto color = consumption > data.upperPowerLimitWatts
        ? core::colorTheme()->color("red")
        : core::colorTheme()->color("light4");

    const auto nodeData = ViewNodeDataBuilder()
        .withText(PoeSettingsColumn::consumption, text)
        .withData(PoeSettingsColumn::consumption, Qt::TextAlignmentRole, kTextAlign)
        .withData(PoeSettingsColumn::consumption, Qt::ForegroundRole, color)
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

PoeSettingsStatePatch::BoolOptional PoeSettingsReducer::poeOverBudgetChanges(
    const PoeSettingsState& state,
    const nx::vms::api::NetworkBlockData& data)
{
    return data.isInPoeOverBudgetMode == state.showPoeOverBudgetWarning
        ? PoeSettingsStatePatch::BoolOptional()
        : data.isInPoeOverBudgetMode;
}

PoeSettingsStatePatch::BoolOptional PoeSettingsReducer::blockUiChanges(
    const PoeSettingsState& state,
    const bool blockUi)
{
    return blockUi == state.blockUi ? PoeSettingsStatePatch::BoolOptional() : blockUi;
}

PoeSettingsStatePatch::BoolOptional PoeSettingsReducer::showPreloaderChanges(
    const PoeSettingsState& state,
        const bool value)
{
    return value == state.showPreloader ? PoeSettingsStatePatch::BoolOptional() : value;
}

PoeSettingsStatePatch::BoolOptional PoeSettingsReducer::autoUpdatesChanges(
    const PoeSettingsState& state,
    const bool value)
{
    return value == state.autoUpdates ? PoeSettingsStatePatch::BoolOptional() : value;
}

} // namespace nx::vms::client::desktop
