#include "default_pipeline_callbacks.h"

#include <random>
#include <cassert>
#include <stdio.h>

extern "C" {

#include <nvosd.h>
#include <gstnvivameta_api.h>

} // extern "C"

#include "default_pipeline.h"
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_common.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>
#define NX_PRINT_PREFIX "deepstream::defaultCallbacks::"
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/sdk/analytics/rect.h>

namespace nx{
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

GstPadProbeReturn waitForSecondaryGieDoneBufProbe(
    GstPad* /*pad*/,
    GstPadProbeInfo* info,
    gpointer userData)
{
    using namespace nx::vms_server_plugins;
    NX_OUTPUT << __func__ << " Calling secondary GIE probe";
    if (info->type & GST_PAD_PROBE_TYPE_EVENT_BOTH)
    {
        auto event = (GstEvent*) info->data;

        NX_OUTPUT
            << __func__ << " Type of probe info is GST_PAD_PROBE_TYPE_EVENT_BOTH"
            << ", event type is " << (int) event->type;

        if (event->type == GST_EVENT_EOS)
            return GST_PAD_PROBE_OK;
    }

    if (info->type & GST_PAD_PROBE_TYPE_BUFFER)
    {
        auto pipeline = (DefaultPipeline*) userData;
        while (pipeline->state() == GST_STATE_PLAYING
            && GST_OBJECT_REFCOUNT_VALUE(GST_BUFFER(info->data)) > 1
            /* && !pipeline->isStopping() */ /*< TODO: #dmishin flush condition */)
        {
            #if 0
            NX_OUTPUT
                << __func__
                << " Waiting for buffer, buffer reference count is: "
                << GST_OBJECT_REFCOUNT_VALUE(GST_BUFFER(info->data));
            #endif
            g_usleep(/*1ms*/ 1000);
        }

        NX_OUTPUT
            << __func__
            << " Secondary classifiers has processed the buffer, iterating over metadata list.";

        auto buffer = (GstBuffer*) info->data;
        gst_buffer_foreach_meta(buffer, handleDefaultMetadata, userData);
    }

    return GST_PAD_PROBE_OK;
}

gboolean handleDefaultMetadata(GstBuffer* buffer, GstMeta** meta, gpointer userData)
{
    NX_OUTPUT << __func__ << " Calling metadata handler callback";
    auto metadata = *meta;
    const bool hasNvidiaTag = gst_meta_api_type_has_tag(
        metadata->info->api,
        kNvidiaIvaMetadataQuark);

    if (!hasNvidiaTag)
        return true;

    NX_OUTPUT << __func__ << " Handling Nvidia metadata";
    if (((IvaMeta*) metadata)->meta_type != NV_BBOX_INFO)
        return true;

    // TODO: #dmishin consider to handle NV_BBOX_CROP meta_type
    auto bboxes = (BBOX_Params*)((IvaMeta*) metadata)->meta_data;
    if (!bboxes)
        return true;

    auto packet = new nx::sdk::analytics::ObjectMetadataPacket();
    packet->setTimestampUs(GST_BUFFER_PTS(buffer));
    // TODO: #dmishin calculate duration or take it from buffer.
    packet->setDurationUs(ini().deepstreamDefaultMetadataDurationMs * 1000);

    auto pipeline = (deepstream::DefaultPipeline*) userData;
    auto trackingMapper = pipeline->trackingMapper();

    auto frameWidth = pipeline->currentFrameWidth();
    if (frameWidth <= 0)
        frameWidth = ini().defaultFrameWidth;

    auto frameHeight = pipeline->currentFrameHeight();
    if (frameHeight <= 0)
        frameHeight = ini().defaultFrameHeight;

    for (int i = 0; i < (int) bboxes->num_rects; ++i)
    {
        const auto& roiMeta = bboxes->roi_meta[i];
        std::string displayText;
        if (roiMeta.text_params.display_text)
        {
            displayText = roiMeta.text_params.display_text;
            NX_OUTPUT << "DISPLAY TEXT: "
                << displayText
                << " " << roiMeta.text_params.display_text;
        }

        auto detectedObject = nx::sdk::makePtr<nx::sdk::analytics::ObjectMetadata>();
        nx::sdk::analytics::Rect rectangle;

        rectangle.x = roiMeta.rect_params.left / (double) frameWidth;
        rectangle.y = roiMeta.rect_params.top / (double) frameHeight;
        rectangle.width = roiMeta.rect_params.width / (double) frameWidth;
        rectangle.height = roiMeta.rect_params.height / (double) frameHeight;

        NX_OUTPUT
            << __func__ << " Got rectangle: "
            << "x: " << rectangle.x << ", "
            << "y: " << rectangle.y << ", "
            << "width: " << rectangle.width << ", "
            << "height: " << rectangle.height;

        std::deque<nx::sdk::Ptr<nx::sdk::analytics::Attribute>> attributes;

        attributes = trackingMapper->attributes(roiMeta);
        nx::sdk::Uuid uuid = trackingMapper->getMapping(roiMeta.tracking_id);
        if (uuid.isNull())
        {
            uuid = nx::sdk::UuidHelper::randomUuid();
            trackingMapper->addMapping(roiMeta.tracking_id, uuid);
        }

        detectedObject->setId(uuid);
        detectedObject->setBoundingBox(rectangle);
        detectedObject->setConfidence(1.0);

        const auto& objectClassDescriptions = pipeline->objectClassDescriptions();
        const auto objectClassId = roiMeta.class_id;

        if (objectClassId < (int) objectClassDescriptions.size())
        {
            detectedObject->setTypeId(objectClassDescriptions[objectClassId].typeId);
            attributes.emplace_front(
                nx::sdk::makePtr<nx::sdk::analytics::Attribute>(
                    nx::sdk::IAttribute::Type::string,
                    "Type",
                    objectClassDescriptions[objectClassId].name));
        }
        else
        {
            detectedObject->setTypeId("");
        }

        if (ini().showGuids)
        {
            attributes.emplace_front(
                nx::sdk::makePtr<nx::sdk::analytics::Attribute>(
                    nx::sdk::IAttribute::Type::string,
                    "GUID",
                    nx::sdk::UuidHelper::toStdString(uuid)));
        }

        detectedObject->addAttributes(
            std::vector<nx::sdk::Ptr<nx::sdk::analytics::Attribute>>(
                attributes.begin(),
                attributes.end()));

        packet->addItem(detectedObject.get());
    }

    pipeline->handleMetadata(packet);
    return true;
}

} // namespace deepstream
} // namespace vms_server_plugins
} // namespace analytics
} // namespace nx
