#pragma once

#include <thread>
#include <atomic>
#include <memory>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_video_frame_processing_camera_manager.h>

#include "plugin.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

class CameraManager: public nx::sdk::metadata::CommonVideoFrameProcessingCameraManager
{
public:
    CameraManager(Plugin* plugin);
    virtual ~CameraManager();

    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* typeList, int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

protected:
    virtual std::string capabilitiesManifest() override;

    virtual bool pushVideoFrame(
        const sdk::metadata::CommonCompressedVideoPacket* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<sdk::metadata::MetadataPacket*>* metadataPackets) override;

    virtual void executeAction(
        const std::string& actionId,
        const nx::sdk::metadata::Object* object,
        const std::map<std::string, std::string>& params,
        std::string* outActionUrl,
        std::string* outMessageToUser,
        nx::sdk::Error* error) override;

private:
    nx::sdk::metadata::MetadataPacket* cookSomeEvents();
    nx::sdk::metadata::MetadataPacket* cookSomeObjects();

    int64_t usSinceEpoch() const;

private:
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_stopping{false};
    int m_counter = 0;
    int m_counterObjects = 0;
    nxpl::NX_GUID m_eventTypeId;
    const sdk::metadata::CommonCompressedVideoPacket* m_videoFrame;
};

const nxpl::NX_GUID kLineCrossingEventGuid =
    {{0x7E,0x94,0xCE,0x15,0x3B,0x69,0x47,0x19,0x8D,0xFD,0xAC,0x1B,0x76,0xE5,0xD8,0xF4}};

const nxpl::NX_GUID kObjectInTheAreaEventGuid =
    {{0xB0,0xE6,0x40,0x44,0xFF,0xA3,0x4B,0x7F,0x80,0x7A,0x06,0x0C,0x1F,0xE5,0xA0,0x4C}};

const nxpl::NX_GUID kCarObjectGuid =
    {{0x15,0x3D,0xD8,0x79,0x1C,0xD2,0x46,0xB7,0xAD,0xD6,0x7C,0x6B,0x48,0xEA,0xC1,0xFC}};

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx