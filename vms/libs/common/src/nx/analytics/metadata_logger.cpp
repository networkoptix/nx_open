#include "metadata_logger.h"

#include <utils/common/synctime.h>
#include <nx/utils/placeholder_binder.h>
#include <nx/utils/debug_helpers/debug_helpers.h>
#include <nx/analytics/analytics_logging_ini.h>

namespace nx::analytics {

using namespace std::chrono;

namespace {

const PlaceholderName kCurrentTimePlaceholder("current_time");
const PlaceholderName kFrameTimestampPlaceholder("frame_timestamp");
const PlaceholderName kFrameTimeDifferenceFromCurrentTime(
        "frame_time_difference_from_current_time");
const PlaceholderName kFrameTimeDifferenceFromPreviousFrame(
        "frame_time_difference_from_previous_frame");

const PlaceholderName kMetadataTimestampPlaceholder("metadata_timestamp");
const PlaceholderName kMetadataTimeDifferenceFromCurrentTime(
        "metadata_time_difference_from_current_time");

const PlaceholderName kMetadataTimeDifferenceFromPreviousMetadata(
        "metadata_time_differnce_from_previous_metadata");

const PlaceholderName kMetadataTimeDifferenceFromLastFrame(
        "metadata_time_difference_from_last_frame");

} // namespace

MetadataLogger::MetadataLogger(
    const QString& logFilePrefix,
    const QString& frameLogPattern,
    const QString& metadataLogPattern,
    QnUuid deviceId,
    QnUuid engineId)
    :
    m_frameLogPattern(frameLogPattern),
    m_metadataLogPattern(metadataLogPattern)
{
    if (!loggingIni().isLoggingEnabled())
        return;

    const QString logFileName = makeLogFileName(
        loggingIni().analyticsLogPath,
        logFilePrefix,
        deviceId,
        engineId);

    if (logFileName.isEmpty())
        return;

    m_outputFile.setFileName(logFileName);
    if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        NX_WARNING(this, "Unable to open output file %1 for logging", logFileName);
}

void MetadataLogger::pushFrameInfo(std::unique_ptr<IFrameInfo> frameInfo)
{
    if (!loggingIni().isLoggingEnabled())
        return;

    m_previousFrameTimestamp = m_lastFrameTimestamp;
    m_lastFrameTimestamp = frameInfo->timestamp();

    doLogging(m_frameLogPattern);
}

void MetadataLogger::pushObjectMetadata(
        const nx::common::metadata::DetectionMetadataPacket& metadataPacket)
{
    if (!loggingIni().isLoggingEnabled())
        return;

    m_previousObjectMetadataPacket = std::move(m_lastObjectMetadataPacket);
    m_lastObjectMetadataPacket = metadataPacket;

    doLogging(m_metadataLogPattern);
}

PlaceholderMap MetadataLogger::placeholderMap() const
{
    PlaceholderMap result;
    microseconds currentTime(qnSyncTime->currentUSecsSinceEpoch());

    return {
        {
            kCurrentTimePlaceholder,
            QString::number(duration_cast<milliseconds>(currentTime).count())
        },
        {
            kFrameTimestampPlaceholder,
            QString::number(duration_cast<milliseconds>(m_lastFrameTimestamp).count())
        },
        {
            kFrameTimeDifferenceFromPreviousFrame,
            QString::number(duration_cast<milliseconds>(
                m_lastFrameTimestamp - m_previousFrameTimestamp).count())
        },
        {
            kFrameTimeDifferenceFromCurrentTime,
            QString::number(duration_cast<milliseconds>(
                m_lastFrameTimestamp - currentTime).count())
        },
        {
            kMetadataTimestampPlaceholder,
            QString::number(m_lastObjectMetadataPacket.timestampUsec / 1000)
        },
        {
            kMetadataTimeDifferenceFromCurrentTime,
            QString::number(duration_cast<milliseconds>(
                microseconds(m_lastObjectMetadataPacket.timestampUsec) - currentTime).count())
        },
        {
            kMetadataTimeDifferenceFromPreviousMetadata,
            QString::number((m_lastObjectMetadataPacket.timestampUsec
                - m_previousObjectMetadataPacket.timestampUsec) / 1000)
        },
        {
            kMetadataTimeDifferenceFromLastFrame,
            QString::number(duration_cast<milliseconds>(
                microseconds(m_lastObjectMetadataPacket.timestampUsec) - m_lastFrameTimestamp)
                    .count())
        }
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
    if (logLine[logLine.length() - 1] != '\n')
        logLine.append('\n');

    m_outputFile.write(logLine.toUtf8());
}

QString MetadataLogger::makeLogFileName(
    const QString& analyticsLoggingPath,
    const QString& logFilePrefix,
    QnUuid deviceId,
    QnUuid engineId)
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

    fileName.append("_");
    fileName.append(QString::number((std::uintptr_t) this));
    fileName.append(".log");

    const QString logDirectoryPath = nx::utils::debug_helpers::debugFilesDirectoryPath(
        loggingIni().analyticsLogPath);

    if (logDirectoryPath.isEmpty())
        return QString();

    const QDir dir(logDirectoryPath);
    return dir.absoluteFilePath(fileName);
}

} // namespace nx::analytics
