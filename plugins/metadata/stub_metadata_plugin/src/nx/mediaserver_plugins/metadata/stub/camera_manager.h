#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>

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
        const char* const* typeList, int typeListSize) override;

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
    std::condition_variable m_eventGenerationLoopCondition;
    std::mutex m_eventGenerationLoopMutex;

    bool m_previewAttributesGenerated = false;
    int m_frameCounter = 0;
    int m_counter = 0;
    int m_objectCounter = 0;
    std::string m_eventTypeId;
    int64_t m_lastVideoFrameTimestampUsec = 0;
};

const std::string kLineCrossingEventType = "nx.stub.lineCrossing";
const std::string kObjectInTheAreaEventType = "nx.stub.objectInTheArea";
const std::string kCarObjectType = "nx.stub.car";
const std::string kHumanFaceObjectType = "nx.stub.humanFace";

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
