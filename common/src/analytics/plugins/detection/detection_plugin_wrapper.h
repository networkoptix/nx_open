#pragma once

#include <detection_plugin_interface/detection_plugin_interface.h>

#include <utils/media/frame_info.h>
#include <nx/utils/uuid.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/media_data_packet.h>
#include <analytics/common/video_metadata_plugin.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace analytics {

class NaiveObjectTracker;

class DetectionPluginWrapper: public VideoMetadataPlugin
{
public:
    DetectionPluginWrapper();
    ~DetectionPluginWrapper();

    virtual QString id() const override;
    virtual bool hasMetadata() override;

    virtual bool pushFrame(const CLVideoDecoderOutputPtr& decodedFrame) override;
    virtual bool pushFrame(const QnCompressedVideoDataPtr& compressedFrame) override;

    virtual QnAbstractCompressedMetadataPtr getNextMetadata() override;

    virtual void reset() override;

    virtual void setDetector(std::unique_ptr<AbstractDetectionPlugin> detector);

private:
    static const int kMaxRectanglesNumber = 40;

private:
    QnObjectDetectionMetadataPtr rectsToObjectDetectionMetadata(
        const AbstractDetectionPlugin::Rect* rectangles,
        int rectangleCount) const;

private:
    mutable std::unique_ptr<NaiveObjectTracker> m_objectTracker;
    std::unique_ptr<AbstractDetectionPlugin> m_detector;
    AbstractDetectionPlugin::Rect m_detections[kMaxRectanglesNumber];
};

} // namespace analytics
} // namespace nx
