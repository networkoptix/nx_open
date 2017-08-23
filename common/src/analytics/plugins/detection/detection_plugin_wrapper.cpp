#include "detection_plugin_wrapper.h"

#include <QtCore/QFile>
#include <QtCore/QLibrary>

#include <limits.h>
#include <cmath>
#include <fstream>
#include <ios>

#define NX_OUTPUT_PREFIX "[analytics::DetectionPlugin] "
#include <nx/kit/debug.h>

#include <nx/utils/log/log.h>

#include <utils/common/synctime.h>
#include <analytics/plugins/detection/config.h>
#include <analytics/plugins/detection/naive_object_tracker.h>

namespace nx {
namespace analytics {

namespace {

QByteArray loadFileToByteArray(const QString& filename)
{
    QByteArray result;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_OUTPUT << "Unable to open file for reading: [" << filename.toStdString() << "]";
        return result;
    }

    result = file.readAll();
    file.close();

    if (result.isEmpty())
        NX_OUTPUT << "Unable to read from file: " << filename.toStdString();

    NX_OUTPUT << "Substituted frame from file: " << filename.toStdString();
    return result;
}

const int kMaxIdLength = 255;

} // namespace

DetectionPluginWrapper::DetectionPluginWrapper():
    m_objectTracker(new NaiveObjectTracker())
{
}

DetectionPluginWrapper::~DetectionPluginWrapper()
{
    m_detector.reset();
}

QString DetectionPluginWrapper::id() const
{
    if (!m_detector)
        return QString();

    char id[kMaxIdLength];
    m_detector->id(id, kMaxIdLength);
    return QString::fromStdString(std::string(id, kMaxIdLength));
}

bool DetectionPluginWrapper::hasMetadata()
{
    if (!m_detector)
        return false;

    return m_detector->hasMetadata();
}

bool DetectionPluginWrapper::pushFrame(const CLVideoDecoderOutputPtr& data)
{
    NX_ASSERT(false, "Not implemented!");
    return false;
}

bool DetectionPluginWrapper::pushFrame(const QnCompressedVideoDataPtr& compressedFrameData)
{
    if (!m_detector)
        return false;

    AbstractDetectionPlugin::CompressedFrame compressedFrame;
    QByteArray dataFromFile;

    if (ini().substituteFramesFilePrefix[0] != '\0')
    {
        // Instead of the received data, read a raw frame from the file.
        static int frameNumber = 0;
        ++frameNumber;

        dataFromFile = loadFileToByteArray(
                lit("%1.%2")
                    .arg(QLatin1String(ini().substituteFramesFilePrefix))
                    .arg(frameNumber));

        compressedFrame.data = (const uint8_t*) dataFromFile.constData();
        compressedFrame.dataSize = dataFromFile.size();
    }
    else
    {
        compressedFrame.data = (const uint8_t*) compressedFrameData->data();
        compressedFrame.dataSize = (int) compressedFrameData->dataSize();   
    }

    compressedFrame.ptsUs = compressedFrameData->timestamp;

    if (!m_detector->pushCompressedFrame(&compressedFrame))
    {
        NX_PRINT << "ERROR: pushCompressedFrame(dataSize: " << compressedFrame.dataSize
            << ", ptsUs: " << compressedFrame.ptsUs << ") -> false";
        return false;
    }
    return true;
}

QnAbstractCompressedMetadataPtr DetectionPluginWrapper::getNextMetadata()
{
    if (!m_detector)
        return nullptr;

    QnObjectDetectionMetadataPtr result;
    int64_t pts;
    int count = 0;

    m_detector->pullRectsForFrame(m_detections, kMaxRectanglesNumber, &count, &pts);
    result = rectsToObjectDetectionMetadata(m_detections, count);

    if (result)
    {
        auto compressed = result->serialize();
        compressed->timestamp = pts > 0 ? pts : qnSyncTime->currentMSecsSinceEpoch();
        compressed->channelNumber = m_channel;
        compressed->m_duration = 1000 * 1000 * 1000; //< 1000 seconds.
        return compressed;
    }

    return nullptr;
}

void DetectionPluginWrapper::reset()
{
    m_detector.reset();
}

void DetectionPluginWrapper::setDetector(std::unique_ptr<AbstractDetectionPlugin> detector)
{
    m_detector = std::move(detector);
}

QnObjectDetectionMetadataPtr DetectionPluginWrapper::rectsToObjectDetectionMetadata(
    const AbstractDetectionPlugin::Rect* rectangles, int rectangleCount) const
{
    static QElapsedTimer timer;
    timer.restart();

    QnObjectDetectionMetadataPtr metadata =
        std::make_shared<QnObjectDetectionMetadata>();

    NX_OUTPUT << "---------------------------------------------------------";
    NX_OUTPUT << "----------------- GOT RECTANGLES: " << rectangleCount;
    for (auto i = 0; i < rectangleCount; ++i)
    {
        const auto& rect = rectangles[i];
        QnObjectDetectionInfo info;
        QnPoint2D topLeft(rect.x, rect.y);
        QnPoint2D bottomRight(rect.x + rect.width, rect.y + rect.height);
        info.boundingBox = QnRect(topLeft, bottomRight);
        metadata->detectedObjects.push_back(info);
    }

    m_objectTracker->filterAndTrack(metadata);

    for (const auto& obj: metadata->detectedObjects)
    {
        NX_OUTPUT << "OBJECT ID: " << obj.objectId.toStdString() << " ("
            << obj.boundingBox.topLeft.x << " "
            << obj.boundingBox.topLeft.y << ") ("
            << obj.boundingBox.bottomRight.x << " "
            << obj.boundingBox.bottomRight.y << ")";
    }
    NX_OUTPUT << "-------------------------------------------------------";

    return metadata;
}

} // namespace analytics
} // namespace nx
