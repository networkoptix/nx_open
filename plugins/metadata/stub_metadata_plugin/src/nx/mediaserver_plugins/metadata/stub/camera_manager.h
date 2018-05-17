#pragma once

#include <thread>
#include <atomic>
#include <memory>

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

    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* typeList, int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

protected:
    virtual std::string capabilitiesManifest() override;

    virtual void settingsChanged() override;

    virtual bool pushCompressedVideoFrame(
        const sdk::metadata::CompressedVideoPacket* videoFrame) override;

    virtual bool pushUncompressedVideoFrame(
        const sdk::metadata::UncompressedVideoFrame* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<sdk::metadata::MetadataPacket*>* metadataPackets) override;

private:
    virtual Plugin* plugin() const override
    {
        return dynamic_cast<Plugin*>(CommonVideoFrameProcessingCameraManager::plugin());
    }

    nx::sdk::metadata::MetadataPacket* cookSomeEvents();
    nx::sdk::metadata::MetadataPacket* cookSomeObjects();

    int64_t usSinceEpoch() const;

    bool checkFrame(const sdk::metadata::UncompressedVideoFrame* videoFrame) const;

private:
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_stopping{false};
    bool m_previewAttributesGenerated = false;
    int m_frameCounter = 0;
    int m_counter = 0;
    int m_objectCounter = 0;
    nxpl::NX_GUID m_eventTypeId;
    int64_t m_lastVideoFrameTimestampUsec = 0;
};

const nxpl::NX_GUID kDriverGuid =
    {{0xB1,0x4A,0x8D,0x7B,0x80,0x09,0x4D,0x38,0xA6,0x0D,0x04,0x13,0x93,0x45,0x43,0x2E}};
const nxpl::NX_GUID kLineCrossingEventGuid =
    {{0x7E,0x94,0xCE,0x15,0x3B,0x69,0x47,0x19,0x8D,0xFD,0xAC,0x1B,0x76,0xE5,0xD8,0xF4}};
const nxpl::NX_GUID kObjectInTheAreaEventGuid =
    {{0xB0,0xE6,0x40,0x44,0xFF,0xA3,0x4B,0x7F,0x80,0x7A,0x06,0x0C,0x1F,0xE5,0xA0,0x4C}};
const nxpl::NX_GUID kCarObjectGuid =
    {{0x15,0x3D,0xD8,0x79,0x1C,0xD2,0x46,0xB7,0xAD,0xD6,0x7C,0x6B,0x48,0xEA,0xC1,0xFC}};
const nxpl::NX_GUID kHumanFaceObjectGuid =
    {{0xC2,0x3D,0xEF,0x4D,0x04,0xF7,0x4B,0x4C,0x99,0x4E,0x0C,0x0E,0x6E,0x8B,0x12,0xCB}};

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx