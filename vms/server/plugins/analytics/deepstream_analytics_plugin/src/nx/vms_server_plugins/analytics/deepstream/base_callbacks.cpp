#include "base_callbacks.h"

#define NX_PRINT_PREFIX "deepstream::baseCallbacks::"
#include <nx/kit/debug.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

#include "deepstream_analytics_plugin_ini.h"
#include "base_pipeline.h"

namespace nx{
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

void appSourceNeedData(GstElement* appSrc, guint /*unused*/, gpointer userData)
{
    NX_OUTPUT << __func__ << " Running need-data GstAppSrc callback";
    auto pipeline = (deepstream::BasePipeline*) userData;
    const auto frame = nx::sdk::toPtr(pipeline->nextDataPacket());
    if (!frame)
    {
        NX_OUTPUT << __func__ << " No data available in the frame queue";
        return;
    }

    const auto video = nx::sdk::queryInterfacePtr<nx::sdk::analytics::ICompressedVideoPacket>(
        frame, nx::sdk::analytics::IID_CompressedVideoPacket);
    if (!video)
    {
        NX_OUTPUT << __func__ << " Can not convert data packet to 'ICompressedVideoPacket'";
        return;
    }

    NX_OUTPUT << __func__ << " Allocating buffer of size " << video->dataSize();
    auto buffer = gst_buffer_new_allocate(NULL, video->dataSize(), NULL);

    NX_OUTPUT << __func__ << " Setting biffer PTS to " << video->timestampUs();
    GST_BUFFER_PTS(buffer) = video->timestampUs();

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

    if (result != GST_FLOW_OK)
    {
        NX_PRINT << __func__ << " Error while pushing buffer";
        // TODO: #dmishin handle error somehow.
    }
}

void connectPads(GstElement* /*source*/, GstPad* newPad, gpointer userData)
{
    NX_OUTPUT << __func__ << " Connecting pads on 'pad-added' signal";
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

    NX_OUTPUT << "Pad succesfully linked" << std::endl;
}

gboolean busCallback(GstBus* /*bus*/, GstMessage* message, gpointer userData)
{
    NX_OUTPUT << __func__ << " Calling GStreamer bus callback";
    auto pipeline = (deepstream::BasePipeline*) userData;
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
            NX_OUTPUT << " GOT STATE CHANGED MESSAGE: "
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

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
