// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_tracking_instrument.h"

#include <QtWidgets/QGraphicsItem>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/io_ports/io_ports_compatibility_interface.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/event/action_parameters.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/common/weak_pointer.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>

using namespace nx::vms::client::desktop;

using ExtendedCameraOutput = nx::vms::api::ExtendedCameraOutput;

namespace {

constexpr int kClickDelayMSec = 300;

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
        [](QGraphicsView* /*view*/, QGraphicsItem* item, ClickInfo info)
        {
            if (info.modifiers != Qt::AltModifier)
                return;

            auto widget = qobject_cast<QnMediaResourceWidget*>(item->toGraphicsObject());
            if (!widget)
                return;

            auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return;

            QPointF relatedPos = calculateRelativePosition(info.scenePos, item);
            nx::vms::api::ResolutionData targetLockResolutionData{relatedPos.x(), relatedPos.y()};

            auto systemContext = camera->systemContext()->as<nx::vms::client::core::SystemContext>();

            if (!systemContext->ioPortsInterface())
                return;

            systemContext->ioPortsInterface()->setIoPortState(
                systemContext->getSessionTokenHelper(),
                camera,
                ExtendedCameraOutput::autoTracking,
                /*isActive*/ true,
                /*autoResetTimeout*/ {},
                targetLockResolutionData);
        });
}

ObjectTrackingInstrument::~ObjectTrackingInstrument()
{
    ensureUninstalled();
}
