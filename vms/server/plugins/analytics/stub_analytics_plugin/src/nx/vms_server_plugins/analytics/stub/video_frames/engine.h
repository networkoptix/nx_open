// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/integration.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace video_frames {

class Engine: public nx::sdk::analytics::Engine
{
public:
    using PixelFormat = nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat;

    Engine(nx::sdk::analytics::Integration* integration);

    virtual ~Engine() override;

    // Capabilities.
    bool needUncompressedVideoFrames() const { return m_needUncompressedVideoFrames; }

    PixelFormat pixelFormat() const { return m_pixelFormat; }

    nx::sdk::analytics::Integration* const integration() const { return m_integration; }

protected:
    virtual std::string manifestString() const override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    void initCapabilities();

private:
    nx::sdk::analytics::Integration* const m_integration;

    std::string m_capabilities;
    std::string m_streamTypeFilter;
    bool m_needUncompressedVideoFrames = false;
    PixelFormat m_pixelFormat = PixelFormat::yuv420;
};

} // namespace video_frames
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
