// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_tracking_instrument.h"

#include <QtWidgets/QGraphicsItem>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/event/action_parameters.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/common/weak_pointer.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>

using ExtendedCameraOutput = nx::vms::api::ExtendedCameraOutput;

namespace {

constexpr int kClickDelayMSec = 300;

struct Coordinates
{
    qreal x = 0;
    qreal y = 0;
};
NX_REFLECTION_INSTRUMENT(Coordinates, (x)(y));

QPointF calculateRelativePosition(const QPointF& scenePos, QGraphicsItem* item)
{
    const QRectF rect = item->sceneBoundingRect();
    return {scenePos.x() / rect.width(), scenePos.y() / rect.height()};
}

} // namespace

ObjectTrackingInstrument::ObjectTrackingInstrument(QObject* parent):
    base_type(Qt::LeftButton, kClickDelayMSec, Instrument::Item, parent)
{
    connect(
        this,
        &ClickInstrument::itemClicked,
        this,
        [this](QGraphicsView* /*view*/, QGraphicsItem* item, ClickInfo info)
        {
            if (info.modifiers != Qt::AltModifier)
                return;

            auto widget = qobject_cast<QnMediaResourceWidget*>(item->toGraphicsObject());
            if (!widget)
                return;

            auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return;

            if (!camera->extendedOutputs().testFlag(ExtendedCameraOutput::autoTracking))
                return;

            nx::vms::event::ActionParameters actionParameters;

            actionParameters.durationMs = 0;

            const auto autoTrackingOutputName = QString::fromStdString(
                nx::reflect::toString(ExtendedCameraOutput::autoTracking));

            for (const QnIOPortData& portData: camera->ioPortDescriptions())
            {
                if (portData.outputName == autoTrackingOutputName)
                {
                    actionParameters.relayOutputId = portData.id;
                    break;
                }
            }

            if (actionParameters.relayOutputId.isEmpty())
            {
                NX_WARNING(this, "Unexpected output type.");
                return;
            }

            QPointF relatedPos = calculateRelativePosition(info.scenePos, item);
            Coordinates coords{relatedPos.x(), relatedPos.y()};
            actionParameters.text = QString::fromStdString(nx::reflect::json::serialize(coords));

            nx::vms::api::EventActionData actionData;
            actionData.actionType = nx::vms::api::ActionType::cameraOutputAction;
            actionData.toggleState = nx::vms::api::EventState::active;
            actionData.resourceIds.push_back(camera->getId());
            actionData.params = QJson::serialized(actionParameters);

            auto callback = nx::utils::guarded(this,
                [this](bool success,
                    rest::Handle /*requestId*/,
                    nx::network::rest::JsonResult /*result*/)
                {
                    if (!success)
                        NX_WARNING(this, "Start object tracking request was not successful.");
                });

            if (auto connection = connectedServerApi())
                connection->executeEventAction(actionData, callback, thread());
        });
}

ObjectTrackingInstrument::~ObjectTrackingInstrument()
{
    ensureUninstalled();
}
