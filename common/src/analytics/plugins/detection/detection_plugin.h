#pragma once
#if defined(ENABLE_NVIDIA_ANALYTICS)

#include <analytics/common/abstract_metadata_plugin.h>

#include <QtCore/QElapsedTimer>

#include <boost/optional.hpp>

extern "C" {
#include <libswscale/swscale.h>
} // extern "C"

#include <opencv2/objdetect/objdetect.hpp>

#include <tegra_video.h>

#include <utils/media/frame_info.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/media_data_packet.h>
#include <analytics/common/metadata_packets.h>
#include <analytics/car_detection/gie_executor.h>

#include <analytics/car_detection/car_detection_plugin.h>

namespace nx {
namespace analytics {

class CarDetectionPlugin: public VideoMetadataPlugin
{
public:
    CarDetectionPlugin(const QString& id);
    ~CarDetectionPlugin();
    virtual QString id() const override;
    virtual bool hasMetadata() override;

    virtual bool pushFrame(const CLVideoDecoderOutputPtr& decodedFrame) override;

    virtual bool pushFrame(const QnCompressedVideoDataPtr& compressedFrame) override;

    virtual QnAbstractCompressedMetadataPtr getNextMetadata() override;

    virtual void reset() override;

private:
    static const int kMaxRectanglesNumber = 40;
    static const int kDefaultObjectLifetime = 1;

    struct CachedObject
    {
        QnUuid id;
        QnRect rect;
        int lifetime = kDefaultObjectLifetime;
        QVector2D speed; //< relative units per frame
        bool found = false;
    };

private:
    QnAbstractCompressedMetadataPtr generateFakeMetadata();
    bool convertAndPushImageToGie(const AVFrame* srcFrame);
    QnObjectDetectionMetadataPtr parseGieOutput() const;
    QnObjectDetectionMetadataPtr parseBbox() const;
    QnObjectDetectionMetadataPtr parseHelBbox() const;
    QnObjectDetectionMetadataPtr toObjectDetectionMetadata(
        const std::vector<cv::Rect>& rectList) const;
    QnObjectDetectionMetadataPtr tegraVideoRectsToObjectDetectionMetadata(
        const TegraVideo::Rect* rectangles, int rectangleCount) const;

    boost::optional<CachedObject> findAndMarkSameObjectInCache(const QnRect& boundingBox) const;
    void unmarkFoundObjectsInCache() const;
    void addObjectToCache(const QnUuid& id, const QnRect& boundingBox) const;
    void updateObjectInCache(const QnUuid& id, const QnRect& boundingBox) const;
    void removeExpiredObjectsFromCache() const;
    void addNonExpiredObjectsFromCache(std::vector<QnObjectDetectionInfo>& objects) const;
    QnRect applySpeedToRectangle(const QnRect& rectangle, const QVector2D& speed) const;
    QVector2D calculateSpeed(const QnRect& previousPosition, const QnRect& currentPosition) const;

    double distance(const QnRect& first, const QnRect& second) const;

    QPointF rectangleCenter(const QnRect& rect) const;    
    QnRect correctRectangle(const QnRect& rect) const;   

    double predictXSpeedForRectangle(const QnRect& rect) const; 

private:
    QElapsedTimer m_timer;
    quint32 m_lastFrameTime;
    std::unique_ptr<GieExecutor> m_gieExecutor;
    SwsContext* m_scaleContext;
    uchar* m_scaleBuffer;
    int m_scaleBufferSize;

    bool m_gotData;
    QSize m_prevResolution;
    bool m_hasValidEngine;
    int m_frameCounter;
    mutable std::map<QnUuid, CachedObject> m_cachedObjects;

    std::unique_ptr<TegraVideo> m_tegraVideo;
    TegraVideo::Rect m_tegraVideoRects[kMaxRectanglesNumber];
};

} // namespace analytics
} // namespace nx

#endif // defined(ENABLE_NVIDIA_ANALYTICS)
