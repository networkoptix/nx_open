#include "metadata_logger.h"

#include <QtCore/QDir>

#include <nx/kit/utils.h>

#include <utils/common/synctime.h>
#include <nx/analytics/analytics_logging_ini.h>
#include <nx/utils/debug_helpers/debug_helpers.h>
#include <nx/utils/log/to_string.h>

namespace nx::analytics {

using namespace std::chrono;
using namespace nx::common::metadata;

namespace {

QString makeLogFileName(
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

template<typename Time>
static QString toMsString(Time time)
{
    return QString::number(duration_cast<milliseconds>(time).count());
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

void MetadataLogger::pushData(
    const QnConstAbstractMediaDataPtr& data,
    const QString& additionalInfo)
{
    if (!data)
        return;

    if (!loggingIni().isLoggingEnabled())
        return;

    if (data->dataType == QnAbstractMediaData::DataType::VIDEO)
    {
        pushFrameInfo({microseconds(data->timestamp)}, additionalInfo);
    }
    else if (data->dataType == QnAbstractMediaData::DataType::GENERIC_METADATA)
    {
        const ConstDetectionMetadataPacketPtr objectMetadata =
            fromMetadataPacket(std::dynamic_pointer_cast<const QnCompressedMetadata>(data));

        if (objectMetadata)
            pushObjectMetadata(*objectMetadata, additionalInfo);
    }
}

void MetadataLogger::pushFrameInfo(
    const FrameInfo& frameInfo,
    const QString& additionalFrameInfo)
{
    if (!loggingIni().isLoggingEnabled())
        return;

    log(buildFrameLogString(frameInfo, additionalFrameInfo).toUtf8());
    m_prevFrameTimestamp = frameInfo.timestamp;
}

void MetadataLogger::pushObjectMetadata(
    const DetectionMetadataPacket& metadataPacket,
    const QString& additionalMetadataInfo)
{
    if (!loggingIni().isLoggingEnabled())
        return;

    m_isLoggingBestShot = false;
    if (metadataPacket.objects.size() == 1)
    {
        if (metadataPacket.objects[0].bestShot)
            m_isLoggingBestShot = true;
    }

    log(buildObjectMetadataLogString(metadataPacket, additionalMetadataInfo).toUtf8());
    if (!m_isLoggingBestShot)
        m_prevObjectMetadataPacketTimestamp = microseconds(metadataPacket.timestampUsec);
}

QString MetadataLogger::buildFrameLogString(
    const FrameInfo& frameInfo,
    const QString& additionalInfo)
{
    const microseconds currentTime{qnSyncTime->currentUSecsSinceEpoch()};

    QString result = QString("frameTimestampMs ") + toMsString(frameInfo.timestamp) + ", "
        + "currentTimeMs " + toMsString(currentTime) + ", "
        + "diffFromPrevMs " + toMsString(frameInfo.timestamp - m_prevFrameTimestamp) + ", "
        + "diffFromCurrentTimeMs " + toMsString(frameInfo.timestamp - currentTime);

    if (!additionalInfo.isEmpty())
        result += ", additonalInfo: " + additionalInfo;

    return result;
}

QString MetadataLogger::buildObjectMetadataLogString(
    const DetectionMetadataPacket& metadataPacket,
    const QString& additionalInfo)
{
    const microseconds currentTime{ qnSyncTime->currentUSecsSinceEpoch() };
    const microseconds currentPacketTimestamp = microseconds(metadataPacket.timestampUsec);
    const microseconds diffFromPrev = currentPacketTimestamp - m_prevObjectMetadataPacketTimestamp;

    QString result = QString("metadataTimestampMs ") + toMsString(currentPacketTimestamp) + ", "
        + "currentTimeMs " + toMsString(currentTime) + ", "
        + "diffFromPrevMs " + toMsString(diffFromPrev) + ", "
        + "diffFromCurrentTimeMs " + toMsString(currentPacketTimestamp - currentTime);

    if (!additionalInfo.isEmpty())
        result += ", additonalInfo: " + additionalInfo;

    result += ", objects: " + QString::number(metadataPacket.objects.size());

    if (!loggingIni().logObjectMetadataDetails)
        result += makeObjectsLogLines(metadataPacket.objects);

    return result;
}

void MetadataLogger::log(QString logLine)
{
    if (!logLine.isEmpty() && logLine.back() != '\n')
        logLine.append('\n');

    m_outputFile.write(logLine.toUtf8());
}

} // namespace nx::analytics
