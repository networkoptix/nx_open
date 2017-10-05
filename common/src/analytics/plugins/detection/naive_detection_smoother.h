#pragma once

#include <vector>

#include <boost/optional/optional.hpp>

#include <nx/streaming/media_data_packet.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace analytics {

enum class DetectionEventState
{
    started,
    finished,
    sameState
};

class NaiveDetectionSmoother
{

public:
    NaiveDetectionSmoother();
    DetectionEventState smooth(const QnAbstractCompressedMetadataPtr& metadata);
    void reset();

private:
    DetectionEventState smoothBySlidingWindow(const QnObjectDetectionMetadata& metadata);
    DetectionEventState smoothByActivationSequence(const QnObjectDetectionMetadata& metadata);
    boost::optional<QnObjectDetectionMetadata> decodeMetdata(
        const QnAbstractCompressedMetadataPtr& metadata);

private:
    int m_currentSequenceIndex = 0;
    std::vector<int> m_detectionSequenceMarks;
    int m_positiveSequenceLength = 0;
    int m_negativeSequenceLength = 0;
    bool m_previousHasDetectedObjects = false;
};

} // namespace analytics
} // namespace nx
