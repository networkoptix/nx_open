#include "metadata_logger.h"

#include <QtCore/QDir>

#include <nx/kit/utils.h>

#include <utils/common/synctime.h>
#include <nx/utils/placeholder_binder.h>
#include <nx/utils/debug_helpers/debug_helpers.h>
#include <nx/analytics/analytics_logging_ini.h>

namespace nx::analytics {

using namespace std::chrono;

namespace {

static const QString kCurrentTimeMsPlaceholder = "currentTimeMs";

static const QString kFrameTimestampMsPlaceholder = "frame_timestampMs";
static const QString kFrameDiffFromCurrentTimeMsPlaceholder = "frame_diffFromCurrentTimeMs";
static const QString kFrameDiffFromPrevMsPlaceholder = "frame_diffFromPrevMs";

static const QString kMetadataTimestampMsPlaceholder = "metadata_timestampMs";
static const QString kMetadataDiffFromCurrentTimeMsPlaceholder = "metadata_diffFromCurrentTimeMs";
static const QString kMetadataDiffFromPrevMsPlaceholder = "metadata_diffFromPrevMs";
static const QString kMetadataObjectCountPlaceholder = "metadata_objectCount";
static const QString kMetadataObjectsPlaceholder = "metadata_objects";

static const QString kFrameLogPattern =
    "frameTimestampMs {:" + kFrameTimestampMsPlaceholder + "}, "
    "currentTimeMs {:" + kCurrentTimeMsPlaceholder + "}, "
    "diffFromPrevMs {:" + kFrameDiffFromPrevMsPlaceholder + "}, "
    "diffFromCurrentTimeMs {:" + kFrameDiffFromCurrentTimeMsPlaceholder + "}"
;

static const QString kObjectMetadataLogPattern =
    "metadataTimestampMs {:" + kMetadataTimestampMsPlaceholder + "}, "
    "currentTimeMs {:" + kCurrentTimeMsPlaceholder + "}, "
    "diffFromPrevMs {:" + kMetadataDiffFromPrevMsPlaceholder + "}, "
    "diffFromCurrentTimeMs {:" + kMetadataDiffFromCurrentTimeMsPlaceholder + "}, "
    "objects: {:" + kMetadataObjectCountPlaceholder + "}"
    "{:" + kMetadataObjectsPlaceholder + "}"
;

static QString makeObjectsLogLines(
    const std::vector<nx::common::metadata::DetectedObject>& objects)
{
    static const QString kIndent = "    ";

    if (objects.empty())
        return "";

    QString result = ":\n"; //< The previous line ends with object count.

    for (int i = 0; i < (int) objects.size(); ++i)
    {
        const auto& object = objects.at(i);
        result.append(kIndent);

        result.append(toString(object));

        if (i < (int) objects.size() - 1) //< Not the last object.
            result.append("\n");
    }

    return result;
}

} // namespace

MetadataLogger::MetadataLogger(
    const QString& logFilePrefix,
    QnUuid deviceId,
    QnUuid engineId,
    nx::vms::api::StreamIndex streamIndex)
    :
    m_streamIndex(streamIndex)
{
    if (!loggingIni().isLoggingEnabled())
        return;

    const QString logFileName = makeLogFileName(
        loggingIni().analyticsLogPath,
        logFilePrefix,
        deviceId,
        engineId,
        streamIndex);

    if (logFileName.isEmpty())
        return;

    m_outputFile.setFileName(logFileName);
    if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Append))
        NX_WARNING(this, "Unable to open output file %1 for logging", logFileName);
}

void MetadataLogger::pushFrameInfo(std::unique_ptr<IFrameInfo> frameInfo)
{
    if (!loggingIni().isLoggingEnabled())
        return;

    m_prevFrameTimestamp = m_currentFrameTimestamp;
    m_currentFrameTimestamp = frameInfo->timestamp();

    doLogging(kFrameLogPattern);
}

void MetadataLogger::pushObjectMetadata(
    nx::common::metadata::DetectionMetadataPacket metadataPacket)
{
    if (!loggingIni().isLoggingEnabled())
        return;

    m_isLoggingBestShot = false;
    if (metadataPacket.objects.size() == 1)
    {
        if (metadataPacket.objects[0].bestShot)
            m_isLoggingBestShot = true;
    }

    if (!m_isLoggingBestShot)
    {
        m_prevObjectMetadataPacket = std::move(m_currentObjectMetadataPacket);
        m_currentObjectMetadataPacket = std::move(metadataPacket);
    }
    else
    {
        m_currentBestShotMetadataPacket = std::move(metadataPacket);
    }

    doLogging(kObjectMetadataLogPattern);
}

MetadataLogger::PlaceholderMap MetadataLogger::placeholderMap() const
{
    PlaceholderMap result;
    const microseconds currentTime(qnSyncTime->currentUSecsSinceEpoch());

    const auto& currentMetadataPacket = m_isLoggingBestShot
        ? m_currentBestShotMetadataPacket
        : m_currentObjectMetadataPacket;

    const auto& previousMetadataPacket = m_isLoggingBestShot
        ? m_currentObjectMetadataPacket
        : m_prevObjectMetadataPacket;

    return {
        {
            kCurrentTimeMsPlaceholder,
            QString::number(duration_cast<milliseconds>(currentTime).count())
        },
        {
            kFrameTimestampMsPlaceholder,
            QString::number(duration_cast<milliseconds>(m_currentFrameTimestamp).count())
        },
        {
            kFrameDiffFromPrevMsPlaceholder,
            QString::number(duration_cast<milliseconds>(
                m_currentFrameTimestamp - m_prevFrameTimestamp).count())
        },
        {
            kFrameDiffFromCurrentTimeMsPlaceholder,
            QString::number(duration_cast<milliseconds>(
                m_currentFrameTimestamp - currentTime).count())
        },
        {
            kMetadataTimestampMsPlaceholder,
            QString::number(currentMetadataPacket.timestampUsec / 1000)
        },
        {
            kMetadataDiffFromCurrentTimeMsPlaceholder,
            QString::number(duration_cast<milliseconds>(
                microseconds(currentMetadataPacket.timestampUsec) - currentTime).count())
        },
        {
            kMetadataDiffFromPrevMsPlaceholder,
            QString::number((currentMetadataPacket.timestampUsec
                - previousMetadataPacket.timestampUsec) / 1000)
        },
        {
            kMetadataObjectCountPlaceholder,
            QString::number(currentMetadataPacket.objects.size())
        },
        {
            kMetadataObjectsPlaceholder,
            makeObjectsLogLines(currentMetadataPacket.objects)
        },
    };
}

void MetadataLogger::doLogging(const QString& logPattern)
{
    if (logPattern.isEmpty())
        return;

    nx::utils::PlaceholderBinder binder(logPattern);
    const auto placeholders = placeholderMap();
    binder.bind(placeholders);

    QString logLine = binder.str();
    if (!logLine.isEmpty() && logLine.back() != '\n')
        logLine.append('\n');

    m_outputFile.write(logLine.toUtf8());
}

QString MetadataLogger::makeLogFileName(
    const QString& analyticsLoggingPath,
    const QString& logFilePrefix,
    QnUuid deviceId,
    QnUuid engineId,
    nx::vms::api::StreamIndex streamIndex)
{
    if (analyticsLoggingPath.isEmpty())
        return QString();

    QString fileName = logFilePrefix;
    if (!deviceId.isNull())
        fileName += QString("device_") + deviceId.toSimpleString();

    if (!engineId.isNull())
    {
        if (!deviceId.isNull())
            fileName.append("_");

        fileName += QString("engine_") + engineId.toSimpleString();
    }

    if (streamIndex != nx::vms::api::StreamIndex::undefined)
    {
        fileName.append("_");
        fileName.append(streamIndex == nx::vms::api::StreamIndex::primary ? "high" : "low");
    }

    fileName.append(".log");

    const QString logDirectoryPath = nx::utils::debug_helpers::debugFilesDirectoryPath(
        loggingIni().analyticsLogPath);

    if (logDirectoryPath.isEmpty())
        return QString();

    const QDir dir(logDirectoryPath);
    return dir.absoluteFilePath(fileName);
}

} // namespace nx::analytics
