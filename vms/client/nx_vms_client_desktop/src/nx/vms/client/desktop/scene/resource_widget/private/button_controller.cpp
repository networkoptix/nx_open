// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "button_controller.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/event/action_parameters.h>
#include <ui/graphics/items/overlays/scrollable_text_items_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

using ExtendedCameraOutput = nx::vms::api::ExtendedCameraOutput;

namespace nx::vms::client::desktop {

ButtonController::ButtonController(QnMediaResourceWidget* mediaResourceWidget):
    base_type(mediaResourceWidget),
    m_parentWidget(mediaResourceWidget),
    m_camera(mediaResourceWidget->resource().dynamicCast<QnVirtualCameraResource>())
{
    if (!m_camera)
        return;

    for (const QnIOPortData& portData: m_camera->ioPortDescriptions())
        m_outputNameToId[portData.outputName] = portData.id;

    m_ioModuleMonitor = mediaResourceWidget->getIOModuleMonitor();

    if (NX_ASSERT(m_ioModuleMonitor))
    {
        connect(m_ioModuleMonitor.get(), &QnIOModuleMonitor::ioStateChanged,
            this, [this](const QnIOStateData& value) { handleChangedIOState(value); },
            Qt::QueuedConnection);
    }
}

ButtonController::~ButtonController()
{
}

void ButtonController::setButtonContainer(QnScrollableItemsWidget* buttonsContainer)
{
    m_buttonsContainer = buttonsContainer;
}

void ButtonController::removeButtons()
{
    for (const nx::Uuid& id: m_outputTypeToButtonId)
    {
        if (!id.isNull())
            m_buttonsContainer->deleteItem(id);
    }

    m_outputTypeToButtonId.clear();
}

void ButtonController::ensureButtonState(
    ExtendedCameraOutput outputType,
    std::function<GraphicsWidget*()> createButton)
{
    if (m_camera->extendedOutputs().testFlag(outputType))
        createButtonIfNeeded(outputType, createButton);
    else
        removeButton(outputType);
}

void ButtonController::createButtonIfNeeded(
    ExtendedCameraOutput outputType,
    std::function<GraphicsWidget*()> createButton)
{
    if (!m_outputTypeToButtonId.contains(outputType))
        m_outputTypeToButtonId[outputType] = m_buttonsContainer->addItem(createButton());
}

void ButtonController::removeButton(ExtendedCameraOutput outputType)
{
    auto iter = m_outputTypeToButtonId.find(outputType);
    if (iter != m_outputTypeToButtonId.end())
    {
        m_buttonsContainer->deleteItem(iter.value());
        m_outputTypeToButtonId.erase(iter);
    }
}

void ButtonController::buttonClickHandler(
    GraphicsWidget* button,
    ExtendedCameraOutput outputType,
    nx::vms::api::EventState newState)
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

    nx::vms::api::EventActionData actionData;
    actionData.actionType = nx::vms::api::ActionType::cameraOutputAction;
    actionData.toggleState = newState;
    actionData.resourceIds.push_back(m_camera->getId());
    actionData.params = QJson::serialized(actionParameters);

    auto callback = nx::utils::guarded(button,
        [this, button](
            bool success,
            rest::Handle requestId,
            nx::network::rest::JsonResult result)
        {
            handleApiReply(button, success, requestId, result);
        });

    auto systemContext = SystemContext::fromResource(m_camera);
    if (auto connection = systemContext->connectedServerApi())
        connection->executeEventAction(actionData, callback, thread());
}

QString ButtonController::getOutputId(nx::vms::api::ExtendedCameraOutput outputType) const
{
    const auto outputName = QString::fromStdString(nx::reflect::toString(outputType));
    if (auto iter = m_outputNameToId.constFind(outputName);
        iter != m_outputNameToId.end())
    {
        return iter.value();
    }

    return "";
}

void ButtonController::openIoModuleConnection()
{
    if (NX_ASSERT(m_ioModuleMonitor) && !m_ioModuleMonitor->connectionIsOpened())
        m_ioModuleMonitor->open();
}

} // namespace nx::vms::client::desktop
