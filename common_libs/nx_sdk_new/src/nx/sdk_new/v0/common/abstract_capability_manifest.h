#pragma once

#include <nx/sdk_new/v0/common/abstract_object.h>
#include <nx/sdk_new/v0/common/guid.h>
#include <nx/sdk_new/v0/common/codec.h>
#include <nx/sdk_new/v0/common/pixel_format.h>
#include <nx/sdk_new/v0/common/resolution.h>
#include <nx/sdk_new/v0/common/frame_handle_type.h>

namespace nx {
namespace sdk {
namespace v0 {

class AbstractCapabilityManifest: public AbstractObject
{
    virtual ~AbstractCapabilityManifest() {}

    virtual bool needInputData() const = 0;

    virtual int vendorFormats(
        nx::sdk::v0::Guid supportedVendorFormats[],
        int maxVendorCount) const = 0;

    virtual int dataTypes(
        nx::sdk::v0::DataPacketType supportedDataTypes[],
        int maxSupportedDataTypes) const = 0;

    virtual int codecs(
        nx::sdk::v0::CodecId supportedCodecs[],
        int maxCodecCount) const = 0;

    virtual int resolutions(
        nx::sdk::v0::Resolution supportedResolutions[],
        int maxResolutionCount) const = 0;

    virtual int pixelFormats(
        nx::sdk::v0::PixelFormat supportedPixelFormats[],
        int maxPixelFormatCount) const = 0;

    virtual int sampleFormats(
        nx::sdk::v0::SampleFormat supportedSampleFormats[],
        int maxSampleFormatCount) const = 0;

    virtual int frameHandleTypes(
        nx::sdk::v0::FrameHandleType supportedFrameHandleTypes[],
        int maxFrameHandleTypes) const = 0;
};

} // namespace v0
} // namespace sdk
} // namespace nx
