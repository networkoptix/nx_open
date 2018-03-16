#include "default_pipeline_callbacks.h"

#include <random>
#include <cassert>
#include <stdio.h>

extern "C" {

#include <nvosd.h>
#include <gstnvivameta_api.h>

} // extern "C"

#include "default_pipeline.h"
#include "deepstream_common.h"
#include "deepstream_metadata_plugin_ini.h"
#define NX_PRINT_PREFIX "deepstream::callbacks::"
#include <nx/kit/debug.h>

#include <nx/sdk/metadata/common_object.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/compressed_video_packet.h>

namespace nx{
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

void appSourceNeedData(GstElement* appSrc, guint /*unused*/, gpointer userData)
{
    NX_OUTPUT << __func__ << " Running need-data GstAppSrc callback";
    auto pipeline = (metadata::deepstream::DefaultPipeline*) userData;
    nxpt::ScopedRef<sdk::metadata::DataPacket> frame(pipeline->nextDataPacket(), false);
    if (!frame)
    {
        NX_OUTPUT << __func__ << " No data available in the frame queue";
        return;
    }

    nxpt::ScopedRef<sdk::metadata::CompressedVideoPacket> video(
        (sdk::metadata::CompressedVideoPacket*) frame->queryInterface(
            sdk::metadata::IID_CompressedVideoPacket));

    if (!video)
    {
        NX_OUTPUT << __func__ << " Can not convert data packet to 'CompressedVideoPacket'";
        return;
    }

    NX_OUTPUT << __func__ << " Allocating buffer of size " << video->dataSize();
    auto buffer = gst_buffer_new_allocate(NULL, video->dataSize(), NULL);

    NX_OUTPUT << __func__ << " Setting biffer PTS to " << video->timestampUsec();
    GST_BUFFER_PTS(buffer) = video->timestampUsec();

    GstMapInfo info;
    auto memory = gst_buffer_peek_memory(buffer, 0);
    gst_memory_map(memory, &info, GST_MAP_WRITE);
    // TODO: #dmishin consider something more efficient than memcpy
    memcpy((void*) info.data, (void*) video->data(), video->dataSize());

    NX_OUTPUT << __func__ << " Pushing buffer to GStreamer pipeline";
    GstFlowReturn result;
    g_signal_emit_by_name(appSrc, "push-buffer", buffer, &result);
    gst_memory_unmap(memory, &info);
    gst_buffer_unref(buffer);

    video->releaseRef();

    if (result != GST_FLOW_OK)
    {
        NX_PRINT << __func__ << " Error while pushing buffer";
        // TODO: #dmishin handle error somehow.
    }
}

GstPadProbeReturn waitForSecondaryGieDoneBufProbe(
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer userData)
{
    using namespace nx::mediaserver_plugins;
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
        auto pipeline = (metadata::deepstream::DefaultPipeline*) userData;
        while (pipeline->state() == GST_STATE_PLAYING
            && GST_OBJECT_REFCOUNT_VALUE(GST_BUFFER(info->data)) > 1
            /*&& !pipeline->isStopping() /*TODO: #dmishin 'flush' condition */)
        {
            #if 0
            NX_OUTPUT
                << __func__
                << " Waiting for buffer, buffer reference count is: "
                << GST_OBJECT_REFCOUNT_VALUE(GST_BUFFER(info->data));
            #endif
            g_usleep(/*1ms*/1000);
        }

        NX_OUTPUT
            << __func__
            << " Secondary classifiers has processed the buffer, iterating over metadata list.";

        auto buffer = (GstBuffer*) info->data;
        gst_buffer_foreach_meta(buffer, metadataHandlerCallback, userData);
    }

    return GST_PAD_PROBE_OK;
}

gboolean metadataHandlerCallback(GstBuffer* buffer, GstMeta** meta, gpointer userData)
{
    NX_OUTPUT << __func__ << " Calling metadata handler callback";
    auto metadata = *meta;
    if (!gst_meta_api_type_has_tag(
            metadata->info->api,
            metadata::deepstream::kNvidiaIvaMetadataQuark))
    {
        return true;
    }

    NX_OUTPUT << __func__ << " Handling Nvidia metadata";
    if (((IvaMeta*) metadata)->meta_type != NV_BBOX_INFO)
        return true;

    // TODO: #dmishin consider to handle NV_BBOX_CROP meta_type
    auto bboxes = (BBOX_Params*)((IvaMeta*) metadata)->meta_data;
    if (!bboxes)
        return true;

    auto packet = new nx::sdk::metadata::CommonObjectsMetadataPacket();
    packet->setTimestampUsec(GST_BUFFER_PTS(buffer));
    packet->setDurationUsec(30000); //< TODO: #dmishin calculate duration or take it from buffer.

    auto pipeline = (metadata::deepstream::DefaultPipeline*) userData;
    auto trackingMapper = pipeline->trackingMapper();

    auto frameWidth = pipeline->currentFrameWidth();
    if (frameWidth <= 0)
        frameWidth = ini().defaultFrameWidth;

    auto frameHeight = pipeline->currentFrameHeight();
    if (frameHeight <= 0)
        frameHeight = ini().defaultFrameHeight;

    for (auto i = 0; i < bboxes->num_rects; ++i)
    {
        const auto& roiMeta = bboxes->roi_meta[i];
        auto detectedObject = new nx::sdk::metadata::CommonObject();
        nx::sdk::metadata::Rect rectangle;

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

        auto guid = trackingMapper->getMapping(roiMeta.tracking_id);
        if (isNull(guid))
        {
            guid = makeGuid();
            trackingMapper->addMapping(roiMeta.tracking_id, guid);
        }

        detectedObject->setId(guid);
        detectedObject->setBoundingBox(rectangle);
        detectedObject->setConfidence(1.0);

        const auto& objectDescriptions = pipeline->objectClassDescriptions();
        const auto objectClassId = roiMeta.class_id;

        if (objectClassId < objectDescriptions.size())
            detectedObject->setTypeId(objectDescriptions[objectClassId].guid);
        else
            detectedObject->setTypeId(kNullGuid);

        auto trackingMapper = pipeline->trackingMapper();
        detectedObject->setAttributes(trackingMapper->attributes(roiMeta));
        packet->addItem(detectedObject);
    }

    pipeline->handleMetadata(packet);
    return true;
}

void decodeBinPadAdded(GstElement* source, GstPad* newPad, gpointer userData)
{
    NX_OUTPUT << __func__ << " Decode bin pad added, trying to link to post-decode pad";
    GstPad* sinkPad = (GstPad*) userData;

    if (gst_pad_is_linked(sinkPad))
    {
        NX_OUTPUT << __func__ << " Pad is already linked";
        return;
    }

    // TODO: #dmishin check the pad caps
    auto result = gst_pad_link(newPad, sinkPad);
    if (GST_PAD_LINK_FAILED(result))
    {
        NX_OUTPUT << __func__ << " Pad link failed";
        return;
    }
}

gboolean busCallback(GstBus* bus, GstMessage* message, gpointer userData)
{
    NX_OUTPUT << __func__ << " Calling GStreamer bus callback";
    auto pipeline = (metadata::deepstream::DefaultPipeline*) userData;
    auto loop = pipeline->mainLoop();

    switch (GST_MESSAGE_TYPE(message))
    {
        case GST_MESSAGE_EOS:
        {
            NX_OUTPUT << __func__ << " Got EOS message, quitting main loop";
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_ERROR:
        {
            gchar* debugInfo;
            GError* error;

            gst_message_parse_error(message, &error, &debugInfo);
            g_free(debugInfo);

            NX_OUTPUT << __func__ << " Got ERROR message: "
                << "Source: " << GST_OBJECT_NAME(message->src) << ", "
                << "Reason: " << error->message
                << ". Quitting main loop";

            g_error_free(error);
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        {
            NX_OUTPUT << " GOT STATE CHANGED MESSAGE"
                    <<  GST_ELEMENT(GST_MESSAGE_SRC(message))
                    << " " << GST_ELEMENT(pipeline->nativeElement());

            if (GST_ELEMENT(GST_MESSAGE_SRC(message)) != GST_ELEMENT(pipeline->nativeElement()))
                break;

            GstState oldState;
            GstState newState;
            gst_message_parse_state_changed(message, &oldState, &newState, NULL);
            switch (newState)
            {
                case GST_STATE_PLAYING:
                    NX_OUTPUT << __func__ << " Got STATE_CHANGED message, new state: PLAYING";
                    GST_DEBUG_BIN_TO_DOT_FILE(
                        GST_BIN(pipeline->nativeElement()),
                        GST_DEBUG_GRAPH_SHOW_ALL,
                        std::string("nx-pipeline-playing").c_str());
                    break;
                case GST_STATE_PAUSED:
                    NX_OUTPUT << __func__ << " Got STATE_CHANGED message, new state: PAUSED";
                    GST_DEBUG_BIN_TO_DOT_FILE(
                        GST_BIN(pipeline->nativeElement()),
                        GST_DEBUG_GRAPH_SHOW_ALL,
                        std::string("nx-pipeline-paused").c_str());
                    break;
                case GST_STATE_READY:
                    NX_OUTPUT << __func__ << " Got STATE_CHANGED message, new state: READY";
                    GST_DEBUG_BIN_TO_DOT_FILE(
                        GST_BIN(pipeline->nativeElement()),
                        GST_DEBUG_GRAPH_SHOW_ALL,
                        std::string("nx-pipeline-ready").c_str());
                    break;
                case GST_STATE_NULL:
                    NX_OUTPUT << __func__ << " Got STATE_CHANGED message, new state: NULL";
                    GST_DEBUG_BIN_TO_DOT_FILE(
                        GST_BIN(pipeline->nativeElement()),
                        GST_DEBUG_GRAPH_SHOW_ALL,
                        std::string("nx-pipeline-null").c_str());
                    break;
                default:
                    NX_OUTPUT << __func__ << "Got STATE_CHANGED message, some unknown state";
                    GST_DEBUG_BIN_TO_DOT_FILE(
                        GST_BIN(pipeline->nativeElement()),
                        GST_DEBUG_GRAPH_SHOW_ALL,
                        std::string("nx-pipeline-unknown").c_str());
                    break;
            }
            break;
        }
        default:
        {
            NX_OUTPUT << __func__ << " Got some unhandled message " << GST_MESSAGE_TYPE(message);
        }
    }

    return true;
}

GstPadProbeReturn primaryGieDoneCallback(
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer userData)
{
    NX_OUTPUT << __func__ << " Calling primary GIE callback, iterating over metadata";
    auto* buffer = (GstBuffer*) info->data;
    gst_buffer_foreach_meta(buffer, metadataHandlerCallback, userData);

    return GST_PAD_PROBE_OK;
}

} // namespace deepstream
} // namespace mediaserver_plugins
} // namespace metadata
} // namespace nx
