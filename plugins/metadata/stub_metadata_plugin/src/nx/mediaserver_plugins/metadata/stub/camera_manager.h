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
    virtual ~CameraManager();

    virtual nx::sdk::Error startFetchingMetadata(
        nxpl::NX_GUID* typeList, int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

protected:
    virtual std::string capabilitiesManifest() override;

    virtual void settingsChanged() override;

    virtual bool pushVideoFrame(
        const sdk::metadata::CommonCompressedVideoPacket* videoFrame) override;

    virtual bool pullMetadataPackets(
        std::vector<sdk::metadata::MetadataPacket*>* metadataPackets) override;

private:
    nx::sdk::metadata::MetadataPacket* cookSomeEvents();
    nx::sdk::metadata::MetadataPacket* cookSomeObjects();

    int64_t usSinceEpoch() const;

private:
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_stopping{false};
    int m_counter = 0;
    int m_counterObjects = 0;
    std::string m_eventTypeId;
    const sdk::metadata::CommonCompressedVideoPacket* m_videoFrame;
};

const std::string kLineCrossingEventGuid = "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}";
const std::string kObjectInTheAreaEventGuid = "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}";
const std::string kCarObjectGuid = "{153DD879-1CD2-46B7-ADD6-7C6B48EAC1FC}";

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx