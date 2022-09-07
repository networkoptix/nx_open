// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_resource_widget.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/layout_resource.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/event/action_parameters.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace {

constexpr auto kHangUpOutputId = nx::vms::api::ExtendedCameraOutput::hangUp;

} // namespace

namespace nx::vms::client::desktop {

IntercomResourceWidget::IntercomResourceWidget(
    QnWorkbenchContext* context,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(context, item, parent)
{
    for (const QnIOPortData& portData: camera()->ioPortDescriptions())
        m_outputNameToId[portData.outputName] = portData.id;

    updateButtonsVisibility();

    auto safeUpdateButtonsVisibility =
        [this, item]
        {
            if (item->layout() && item->layout()->resource())
                updateButtonsVisibility();
        };

    connect(item->layout(), &QnWorkbenchLayout::itemAdded, item, safeUpdateButtonsVisibility);
    connect(item->layout(), &QnWorkbenchLayout::itemRemoved, item, safeUpdateButtonsVisibility);
}

IntercomResourceWidget::~IntercomResourceWidget()
{
    if (camera()->extendedOutputs().testFlag(kHangUpOutputId))
        setOutputPortState(kHangUpOutputId, nx::vms::api::EventState::active);
}

int IntercomResourceWidget::calculateButtonsVisibility() const
{
    int result = base_type::calculateButtonsVisibility();

    const auto intecomResource = resource()->toResourcePtr();
    const auto layout = layoutResource();

    if (nx::vms::common::isIntercomOnIntercomLayout(intecomResource, layout))
    {
        const QnUuid intercomToDeleteId = intecomResource->getId();

        QSet<QnUuid> otherIntercomLayoutItemIds; // Other intercom item copies on the layout.

        const auto intercomLayoutItems = layout->getItems();
        for (const QnLayoutItemData& intercomLayoutItem: intercomLayoutItems)
        {
            const auto itemResourceId = intercomLayoutItem.resource.id;
            if (itemResourceId == intercomToDeleteId && intercomLayoutItem.uuid != uuid())
                otherIntercomLayoutItemIds.insert(intercomLayoutItem.uuid);
        }

        // If single intercom copy is on the layout - removal is forbidden.
        if (otherIntercomLayoutItemIds.isEmpty())
            result &= ~Qn::CloseButton;
    }

    return result;
}

QString IntercomResourceWidget::getOutputId(nx::vms::api::ExtendedCameraOutput outputType) const
{
    const auto outputName = QString::fromStdString(nx::reflect::toString(outputType));

    if (auto iter = m_outputNameToId.constFind(outputName); iter != m_outputNameToId.end())
        return iter.value();

    NX_WARNING(this, "Unexpected output type %1.", outputName);
    return "";
}

void IntercomResourceWidget::setOutputPortState(
    nx::vms::api::ExtendedCameraOutput outputType,
    nx::vms::api::EventState newState) const
{
    nx::vms::event::ActionParameters actionParameters;

    actionParameters.durationMs = 0;
    actionParameters.relayOutputId = getOutputId(outputType);

    if (actionParameters.relayOutputId.isEmpty())
    {
        NX_WARNING(this,
            "Unexpected output type %1.",
            QString::fromStdString(nx::reflect::toString(outputType)));
        return;
    }

    const auto intecomResource = resource()->toResourcePtr();

    nx::vms::api::EventActionData actionData;
    actionData.actionType = nx::vms::api::ActionType::cameraOutputAction;
    actionData.toggleState = newState;
    actionData.resourceIds.push_back(intecomResource->getId());
    actionData.params = QJson::serialized(actionParameters);

    auto callback =
        [this](
            bool /*success*/,
            rest::Handle /*requestId*/,
            nx::network::rest::JsonResult /*result*/)
        {
        };

    if (auto connection = connectedServerApi())
        connection->executeEventAction(actionData, callback, thread());
}

} // namespace nx::vms::client::desktop
