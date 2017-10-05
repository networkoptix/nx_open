#include "naive_detection_smoother.h"

#include <analytics/plugins/detection/config.h>

#define NX_OUTPUT_PREFIX "[nx::analytics::NaiveDetectionSmoother] "
#include <nx/kit/debug.h>

namespace nx {
namespace analytics {

namespace {

static const char kActivationSequenceSmoothing[] = "continuousSequence";
static const char kSlidingWindowSmoothing[] = "slidingWindow";

} // namespace


NaiveDetectionSmoother::NaiveDetectionSmoother()
{
    m_detectionSequenceMarks.resize(ini().slidingWindowSize);
}

DetectionEventState NaiveDetectionSmoother::smooth(
    const QnAbstractCompressedMetadataPtr& metadata)
{
    auto objDetection = decodeMetdata(metadata);
    if (!objDetection)
        return DetectionEventState::sameState;

    auto smoothingFilterType = ini().smoothingFilterType;
    if (strcmp(smoothingFilterType, kActivationSequenceSmoothing) == 0)
        return smoothByActivationSequence(objDetection.get());
    else if (strcmp(smoothingFilterType, kSlidingWindowSmoothing) == 0)
        return smoothBySlidingWindow(objDetection.get());

    NX_ASSERT(
        false,
        lit("Unknown smoothing filter type. %1")
            .arg(QString::fromStdString(std::string(smoothingFilterType))));

    return DetectionEventState::sameState;
}

void NaiveDetectionSmoother::reset()
{
    m_currentSequenceIndex = 0;
    m_detectionSequenceMarks.clear();
    m_detectionSequenceMarks.resize(ini().slidingWindowSize);
    m_positiveSequenceLength = 0;
    m_negativeSequenceLength = 0;
    m_previousHasDetectedObjects = false;
}

DetectionEventState NaiveDetectionSmoother::smoothBySlidingWindow(
    const QnObjectDetectionMetadata& objDetection)
{
    if(m_currentSequenceIndex > ini().slidingWindowSize - 1)
        m_currentSequenceIndex = 0;

    if (objDetection.detectedObjects.empty())
        m_detectionSequenceMarks[m_currentSequenceIndex] = 0;
    else
        m_detectionSequenceMarks[m_currentSequenceIndex] = 1;

    ++m_currentSequenceIndex;

    int detections = 0;
    bool detected = false;
    for (auto& mark: m_detectionSequenceMarks)
        detections += mark;

    const int minDetections =
        ini().slidingWindowSize / 2
        + ini().slidingWindowSize % 2;

    if (detections >= minDetections)
        detected = true;

    NX_OUTPUT << "### Detections: " << detections
        << "; Min detections needed: " << minDetections
        << "; Sequence length: " << ini().slidingWindowSize;

    if (detected && !m_previousHasDetectedObjects)
    {
        m_previousHasDetectedObjects = true;
        return DetectionEventState::started;
    }
    else if (!detected && m_previousHasDetectedObjects)
    {
        m_previousHasDetectedObjects = false;
        return DetectionEventState::finished;
    }

    return DetectionEventState::sameState;
}

DetectionEventState NaiveDetectionSmoother::smoothByActivationSequence(
    const QnObjectDetectionMetadata& objDetection)
{
    if (objDetection.detectedObjects.empty())
    {
        m_positiveSequenceLength = 0;
        ++m_negativeSequenceLength;
    }
    else
    {
        m_negativeSequenceLength = 0;
        ++m_positiveSequenceLength;
    }

    NX_OUTPUT << "### Positive sequence length: " << m_positiveSequenceLength
        << "; Negative sequence length: " << m_negativeSequenceLength
        << "; Had detected objects before: " << m_previousHasDetectedObjects;

    bool detectionActivated = !m_previousHasDetectedObjects
        && m_positiveSequenceLength >= ini().activationSequenceLength;

    if (detectionActivated)
        return DetectionEventState::started;

    bool detectionDeactivated = m_previousHasDetectedObjects
        && m_negativeSequenceLength >= ini().deactivationSequenceLength;

    if (detectionDeactivated)
        return DetectionEventState::finished;

    return DetectionEventState::sameState;
}

boost::optional<QnObjectDetectionMetadata> NaiveDetectionSmoother::decodeMetdata(
    const QnAbstractCompressedMetadataPtr& metadata)
{
    bool metadataTypeIsOk = metadata->dataType != QnAbstractMediaData::DataType::GENERIC_METADATA
        || metadata->metadataType != MetadataType::ObjectDetection;

    if (metadataTypeIsOk)
        return boost::none;

    QnObjectDetectionMetadata objDetection;
    auto compressed = std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);

    if (!compressed || compressed->metadataType != MetadataType::ObjectDetection)
        return boost::none;

    if (!objDetection.deserialize(compressed))
        return boost::none;

    return objDetection;
}

} // namespace analytics
} // namespace nx
