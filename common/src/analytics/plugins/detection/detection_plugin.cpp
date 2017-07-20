#include "detection_plugin.h"
#if defined(ENABLE_NVIDIA_ANALYTICS)

#include <QtCore/QFile>

#include <limits.h>
#include <cmath>
#include <fstream>
#include <ios>

#define OUTPUT_PREFIX "[analytics::CarDetectionPlugin] "
#include <nx/kit/ini_config.h>

#include <utils/common/synctime.h>
#include <analytics/plugins/detection/config.h>

namespace nx {
namespace analytics {

namespace {

QByteArray loadFileToByteArray(const QString& filename)
{
    QByteArray result;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        PRINT << "Unable to open file for reading: [" << filename << "]";
        return result;
    }

    result = file.readAll();
    file.close();

    if (result.isEmpty())
        PRINT << "Unable to read from file: " << filename;

    OUTPUT << "Substituted frame from file: " << filename;
    return result;
}

} // namespace

DetectionPlugin::DetectionPlugin(const QString& id):
    m_tegraVideo(nullptr)
{
    TegraVideo::Params params;
    std::string idStr = id.toStdString();

    params.id = idStr.c_str();
    params.modelFile = conf.modelFile;
    params.deployFile = conf.deployFile;
    params.cacheFile = conf.cacheFile;

    m_tegraVideo.reset(TegraVideo::create(params));
    NX_CRITICAL(m_tegraVideo, "Unable to initialize tegra_video.");
}

DetectionPlugin::~DetectionPlugin()
{
    m_tegraVideo.reset();
}

QString DetectionPlugin::id() const
{
    return m_tegraVideo->id();
}

bool DetectionPlugin::hasMetadata()
{
    return m_tegraVideo->hasMetadata();
}

bool DetectionPlugin::pushFrame(const CLVideoDecoderOutputPtr& data)
{
    NX_ASSERT(false, "Not implemented!");
    return false;
}

bool DetectionPlugin::pushFrame(const QnCompressedVideoDataPtr& compressedFrameData)
{
    TegraVideo::CompressedFrame compressedFrame;
    QByteArray dataFromFile;

    if (conf.substituteFramesFilePrefix[0] != '\0')
    {
        // Instead of the received data, read a raw frame from the file.
        static int frameNumber = 0;
        ++frameNumber;

        dataFromFile = loadFileToByteArray(
                lit("%1.%2")
                    .arg(QLatin1String(conf.substituteFramesFilePrefix))
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

    if (!m_tegraVideo->pushCompressedFrame(&compressedFrame))
    {
        PRINT << "ERROR: pushCompressedFrame(dataSize: " << compressedFrame.dataSize
            << ", ptsUs: " << compressedFrame.ptsUs << ") -> false";
        return false;
    }
    return true;
}

QnAbstractCompressedMetadataPtr DetectionPlugin::getNextMetadata()
{
    QnObjectDetectionMetadataPtr result;
    int64_t pts;
    int count = 0;

    m_tegraVideo->pullRectsForFrame(m_tegraVideoRects, kMaxRectanglesNumber, &count, &pts);
    result = tegraVideoRectsToObjectDetectionMetadata(m_tegraVideoRects, count);

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

void DetectionPlugin::reset()
{
    m_tegraVideo.reset();
}

QnObjectDetectionMetadataPtr DetectionPlugin::tegraVideoRectsToObjectDetectionMetadata(
    const TegraVideo::Rect* rectangles, int rectangleCount) const
{
    static QElapsedTimer timer;
    timer.restart();

    QnObjectDetectionMetadataPtr metadata =
        std::make_shared<QnObjectDetectionMetadata>();

    PRINT << "---------------------------------------------------------";
    PRINT << "----------------- GOT RECTANGLES: " << rectangleCount;
    for (auto i = 0; i < rectangleCount; ++i)
    {
        const auto& rect = rectangles[i];
        QnObjectDetectionInfo info;
        QnPoint2D topLeft(rect.x, rect.y);
        QnPoint2D bottomRight(rect.x + rect.width, rect.y + rect.height);
        info.boundingBox = QnRect(topLeft, bottomRight);
        metadata->detectedObjects.push_back(info);
    }

    m_objectTracker.filterAndTrack(metadata);

    for (const auto& obj: metadata->detectedObjects)
    {
        PRINT << "OBJECT ID: "<< obj.objectId << " ("
            << obj.boundingBox.topLeft.x << " "
            << obj.boundingBox.topLeft.y << ") ("
            << obj.boundingBox.bottomRight.x << " "
            << obj.boundingBox.bottomRight.y << ")";
    }
    PRINT << "-------------------------------------------------------";

    return metadata;
}

} // namespace analytics
} // namespace nx

#endif // defined(ENABLE_NVIDIA_ANALYTICS)
