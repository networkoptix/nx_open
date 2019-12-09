#include "data_converter.h"

#include <nx/sdk/ptr.h>

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/in_stream_compressed_metadata.h>
#include <nx/streaming/uncompressed_video_packet.h>

#include <nx/vms/server/analytics/data_packet_adapter.h>
#include <nx/vms/server/analytics/compressed_video_packet.h>
#include <nx/vms/server/analytics/uncompressed_video_frame.h>
#include <nx/vms/server/analytics/custom_metadata_packet.h>

namespace nx::vms::server::analytics {

using StreamType = nx::vms::api::analytics::StreamType;
using StreamTypes = nx::vms::api::analytics::StreamTypes;

DataConverter::DataConverter(int rotationAngle):
    m_rotationAngle(rotationAngle)
{
}

QnAbstractDataPacketPtr DataConverter::convert(
    const QnAbstractDataPacketPtr& dataPacket,
    const StreamRequirements& requirements)
{
    nx::sdk::Ptr<nx::sdk::analytics::IDataPacket> result;

    const auto requiredStreamTypes = requirements.requiredStreamTypes;

    if (const auto compressedVideo = std::dynamic_pointer_cast<QnCompressedVideoData>(dataPacket);
        compressedVideo && requiredStreamTypes.testFlag(StreamType::compressedVideo))
    {
        NX_VERBOSE(this, "Wrapping compressed frame, timestamp %1 us", compressedVideo->timestamp);
        result = nx::sdk::makePtr<CompressedVideoPacket>(compressedVideo);
    }
    else if (const auto inStreamMetadata =
        std::dynamic_pointer_cast<nx::streaming::InStreamCompressedMetadata>(dataPacket);
        inStreamMetadata && requiredStreamTypes.testFlag(StreamType::metadata))
    {
        NX_VERBOSE(this, "Wrapping metadata, timestamp %1 us, codec %2",
            inStreamMetadata->timestamp, inStreamMetadata->codec());
        result = nx::sdk::makePtr<CustomMetadataPacket>(inStreamMetadata);
    }
    else if (const auto uncompressedVideo =
        std::dynamic_pointer_cast<nx::streaming::UncompressedVideoPacket>(dataPacket);
        uncompressedVideo && requiredStreamTypes.testFlag(StreamType::uncompressedVideo))
    {
        const CLConstVideoDecoderOutputPtr decoderOutput = uncompressedVideo->frame();
        if (!NX_ASSERT(decoderOutput))
            return nullptr;

        NX_VERBOSE(this, "Converting uncompressed frame, timestamp %1 us",
            uncompressedVideo->timestamp);

        return getUncompressedFrame(decoderOutput, requirements);
    }

    if (result)
        return std::make_shared<DataPacketAdapter>(result);

    return nullptr;
}

QnAbstractDataPacketPtr DataConverter::getUncompressedFrame(
    CLConstVideoDecoderOutputPtr decoderOutput,
    const StreamRequirements& requirements)
{
    if (const auto it = m_cachedUncompressedFrames.find(requirements.uncompressedFramePixelFormat);
        it != m_cachedUncompressedFrames.cend())
    {
        NX_VERBOSE(this, "Frame with pixel format %1 has been found in the cache",
            requirements.uncompressedFramePixelFormat);
        return it->second;
    }

    CLConstVideoDecoderOutputPtr rotatedDecoderOutput;
    if (m_rotationAngle)
    {
        NX_VERBOSE(this, "Rotating uncompressed frame %1 degrees clockwise", m_rotationAngle);
        rotatedDecoderOutput =
            CLConstVideoDecoderOutputPtr(decoderOutput->rotated(m_rotationAngle));
    }
    else
    {
        rotatedDecoderOutput = decoderOutput;
    }

    nx::sdk::Ptr<UncompressedVideoFrame> uncompressedSdkVideoFrame;
    if (requirements.uncompressedFramePixelFormat == rotatedDecoderOutput->format)
    {
        NX_VERBOSE(this, "Frame is already has the requested pixel format");
        uncompressedSdkVideoFrame = nx::sdk::makePtr<UncompressedVideoFrame>(rotatedDecoderOutput);
        if (!uncompressedSdkVideoFrame->avFrame())
            return nullptr; //< Error is already logged.
    }
    else
    {
        NX_VERBOSE(this, "Attempting to convert frame pixel format to %1",
            requirements.uncompressedFramePixelFormat);

        uncompressedSdkVideoFrame = nx::sdk::makePtr<UncompressedVideoFrame>(
            rotatedDecoderOutput->width,
            rotatedDecoderOutput->height,
            requirements.uncompressedFramePixelFormat,
            rotatedDecoderOutput->pkt_dts);

        if (!uncompressedSdkVideoFrame->avFrame()
            || !rotatedDecoderOutput->convertTo(uncompressedSdkVideoFrame->avFrame()))
        {
            return nullptr; //< Error is already logged.
        }
    }

    const auto sdkWrapper = std::make_shared<DataPacketAdapter>(uncompressedSdkVideoFrame);
    m_cachedUncompressedFrames[requirements.uncompressedFramePixelFormat] = sdkWrapper;

    return sdkWrapper;
}

} // namespace nx::vms::server::analytics
