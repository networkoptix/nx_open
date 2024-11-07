// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_plugin_types.h>
#include <nx/sdk/interface.h>
#include <plugins/plugin_api.h>

namespace nx::sdk::cloud_storage {

/**
 * Media data packet abstraction. Data and encryptionData are opaque binary data arrays which
 * should not be parsed or processed by the plugin in any way. Media data packet is provided
 * to the plugin by the StreamWriter and is expected to be exactly the same when returned
 * by the StreamReader object.
 */
class IMediaDataPacket: public Interface<IMediaDataPacket>
{
public:
    enum Type
    {
        audio,
        video,
        metadata,
        unknown,
    };

    virtual int64_t timestampUs() const = 0;
    virtual Type type() const = 0;
    virtual const void* data() const = 0;
    virtual unsigned int dataSize() const = 0;
    virtual unsigned int channelNumber() const = 0;
    virtual nxcip::CompressionType codecType() const = 0;
    virtual bool isKeyFrame() const = 0;
    virtual const void* encryptionData() const = 0;
    virtual int encryptionDataSize() const = 0;
};

} // namespace nx::sdk::cloud_storage
