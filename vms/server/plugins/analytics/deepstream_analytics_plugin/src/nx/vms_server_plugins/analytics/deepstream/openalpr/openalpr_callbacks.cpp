#include "openalpr_callbacks.h"

extern "C" {

#include <nvosd.h>
#include <gstnvivameta_api.h>

} // extern "C"

#include <nx/vms_server_plugins/analytics/deepstream/utils.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_common.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>
#define NX_PRINT_PREFIX "deepstream::defaultCallbacks::"
#include <nx/kit/debug.h>

#include <nx/vms_server_plugins/analytics/deepstream/openalpr/openalpr_pipeline.h>
#include <nx/vms_server_plugins/analytics/deepstream/openalpr_common.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/ptr.h>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

gboolean handleOpenAlprMetadata(GstBuffer* buffer, GstMeta** meta, gpointer userData)
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

    if (bboxes->gie_type != 3)
        return true;

    auto packet = new nx::sdk::analytics::ObjectMetadataPacket();
    packet->setTimestampUs(GST_BUFFER_PTS(buffer));
    packet->setDurationUs(30000); //< TODO: #dmishin calculate duration or take it from buffer.

    auto pipeline = (deepstream::OpenAlprPipeline*) userData;
    auto licensePlateTracker = pipeline->licensePlateTracker();

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
        nx::sdk::analytics::IObjectMetadata::Rect rectangle;

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

        std::deque<nx::sdk::analytics::Attribute> attributes;

        const auto& info = licensePlateTracker->licensePlateInfo(displayText);

        auto encodedAttributes = split(displayText, '%');
        for (int i = 0; i < (int) encodedAttributes.size(); ++i)
        {
            switch (i)
            {
                case 0:
                {
                    attributes.emplace_back(
                        nx::sdk::IAttribute::Type::string,
                        "Number",
                        encodedAttributes[i]);
                    break;
                }
                case 1:
                {
                    attributes.emplace_back(
                        nx::sdk::IAttribute::Type::string,
                        "Country",
                        encodedAttributes[i]);
                    break;
                }
                case 2:
                {
                    attributes.emplace_back(
                        nx::sdk::IAttribute::Type::string,
                        "Region",
                        encodedAttributes[i]);
                    break;

                }
                default:
                {
                    NX_OUTPUT << __func__ << " Unknown attribute";
                    break;
                }
            }
        }

        detectedObject->setId(info.uuid);
        detectedObject->setBoundingBox(rectangle);
        detectedObject->setConfidence(1.0);
        detectedObject->setTypeId(kLicensePlateGuid);

        if (ini().showGuids)
        {
            attributes.emplace_front(
                nx::sdk::IAttribute::Type::string,
                "GUID",
                nx::sdk::UuidHelper::toStdString(info.uuid));
        }

        detectedObject->addAttributes(
            std::vector<nx::sdk::analytics::Attribute>(
                attributes.begin(),
                attributes.end()));

        packet->addItem(detectedObject.get());
    }

    pipeline->handleMetadata(packet);
    return true;
}

GstPadProbeReturn processOpenAlprResult(GstPad* /*pad*/, GstPadProbeInfo* info, gpointer userData)
{
    NX_OUTPUT << __func__ << " Calling OpenALPR process result probe";
    auto* buffer = (GstBuffer*) info->data;
    gst_buffer_foreach_meta(buffer, handleOpenAlprMetadata, userData);

    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn dropOpenAlprFrames(GstPad* /*pad*/, GstPadProbeInfo* info, gpointer userData)
{
    NX_OUTPUT << __func__ << " Calling OpenALPR drop frames probe";
    auto pipeline = (deepstream::OpenAlprPipeline*) userData;
    if (!pipeline)
    {
        NX_OUTPUT << __func__ << " Can't convert user data to OpenAlprPipeline";
        return GST_PAD_PROBE_OK;
    }

    if (pipeline->shouldDropFrame(GST_BUFFER_PTS((GstBuffer*)info->data)))
    {
        NX_OUTPUT << __func__ << " Dropping frame";
        return GST_PAD_PROBE_DROP;
    }

    return GST_PAD_PROBE_OK;
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
