#include "poe_settings_table_state_reducer.h"

#include "poe_settings_table_view.h"

#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_constants.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_data_builder.h>

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

QString getConnectedCamera(const NetworkPortState& port, const QnResourcePool& resourcePool)
{
    const auto resource = resourcePool.getResourceById(port.deviceId);
    return resource
        ? resource->getName()
        : PoESettingsTableView::tr("Empty", "In meaning 'There is no camera connected now'");
}

QString getSpeed(const NetworkPortState& port)
{
    return port.linkSpeedMbps > 0 ? QString("%1 Mbps").arg(port.linkSpeedMbps) : "-";
}

QString getConsumption(const NetworkPortState& port)
{
    if (qFuzzyIsNull(port.devicePowerConsumptionWatts) || port.devicePowerConsumptionWatts < 0)
        return "0 W";

    return QString("%1 W / %@ W").arg(
        port.devicePowerConsumptionWatts, port.devicePowerConsumptionLimitWatts);
}

QString getPoweringStatus(const NetworkPortState& port)
{
    switch(port.poweringStatus)
    {
        case PoweringStatus::disconnected:
            return PoESettingsTableView::tr("Disconnected");
        case PoweringStatus::connected:
            return PoESettingsTableView::tr("Connected");
        case PoweringStatus::powered:
            return PoESettingsTableView::tr("Powered");
        default:
            NX_ASSERT(false, "Unexpected network port powering status!");
            return PoESettingsTableView::tr("Unexpected");
    }
}

Qt::CheckState getPowerStatusCheckedState(const NetworkPortState& port)
{
    return port.poweringMode == NetworkPortState::PoweringMode::off
        ? Qt::Unchecked
        : Qt::Checked;
}

NodePtr createPortNodes(
    const nx::vms::api::NetworkBlockData& data,
    const QnResourcePool& resourcePool)
{
    if (data.portStates.empty())
        return NodePtr();

    const auto root = ViewNode::create();
    for (const auto& port: data.portStates)
    {
        root->addChild(ViewNode::create(ViewNodeDataBuilder()
            .withText(PoESettingsColumn::port, QString::number(port.portNumber))
            .withText(PoESettingsColumn::camera, getConnectedCamera(port, resourcePool))

            .withText(PoESettingsColumn::consumption, getConsumption(port))
            .withData(PoESettingsColumn::consumption, progressRole, rand() % 100)
            .withData(PoESettingsColumn::consumption, Qt::TextAlignmentRole, kTextAlign)

            .withText(PoESettingsColumn::speed, getSpeed(port))
            .withText(PoESettingsColumn::status, getPoweringStatus(port))

            .withCheckedState(PoESettingsColumn::power, getPowerStatusCheckedState(port))
            .withData(PoESettingsColumn::power, useSwitchStyleForCheckboxRole, true)
            .data()));
    }
    return root;
}

} // namespace

namespace nx::vms::client::desktop {
namespace settings {

using namespace node_view;
using namespace node_view::details;

NodeViewStatePatch PoESettingsTableStateReducer::applyBlockDataChanges(
    const node_view::details::NodeViewState& state,
    const nx::vms::api::NetworkBlockData& blockData,
    const QnResourcePool& resourcePool)
{
    if (!state.rootNode)
        return NodeViewStatePatch::fromRootNode(createPortNodes(blockData, resourcePool));

    return NodeViewStatePatch();
}


} // namespace settings
} // namespace nx::vms::client::desktop
