#pragma once
#if defined(ENABLE_NVIDIA_ANALYTICS)

#include <tegra_video.h>

#include <utils/media/frame_info.h>
#include <nx/utils/uuid.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/media_data_packet.h>
#include <analytics/common/abstract_metadata_plugin.h>
#include <analytics/common/object_detection_metadata.h>
#include <analytics/plugins/detection/naive_object_tracker.h>

namespace nx {
namespace analytics {

class DetectionPlugin: public VideoMetadataPlugin
{
public:
    DetectionPlugin(const QString& id);
    ~DetectionPlugin();

    virtual QString id() const override;
    virtual bool hasMetadata() override;

    virtual bool pushFrame(const CLVideoDecoderOutputPtr& decodedFrame) override;
    virtual bool pushFrame(const QnCompressedVideoDataPtr& compressedFrame) override;

    virtual QnAbstractCompressedMetadataPtr getNextMetadata() override;

    virtual void reset() override;

private:
    static const int kMaxRectanglesNumber = 40;

private:
    QnObjectDetectionMetadataPtr tegraVideoRectsToObjectDetectionMetadata(
        const TegraVideo::Rect* rectangles,
        int rectangleCount) const;

private:
    NaiveObjectTracker m_objectTracker;
    std::unique_ptr<TegraVideo> m_tegraVideo;
    TegraVideo::Rect m_tegraVideoRects[kMaxRectanglesNumber];
};

} // namespace analytics
} // namespace nx

#endif // defined(ENABLE_NVIDIA_ANALYTICS)
