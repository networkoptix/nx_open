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

/** @return Empty string on error, having logged the error message. */
QString makeLogFileName(
    const QString& analyticsLoggingPath,
    const QString& logFilePrefix,
    QnUuid deviceId,
    QnUuid engineId,
    nx::vms::api::StreamIndex streamIndex)
{
    if (!NX_ASSERT(!analyticsLoggingPath.isEmpty()))
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
        analyticsLoggingPath);

    if (logDirectoryPath.isEmpty())
        return QString();

    const QDir dir(logDirectoryPath);
    return dir.absoluteFilePath(fileName);
}

static QString makeObjectsLogLinesIfNeeded(
    const std::vector<ObjectMetadata>& objectMetadataList)
{
    if (!loggingIni().logObjectMetadataDetails || objectMetadataList.empty())
        return "";

    static const QString kIndent = "    ";

    QString result = ":\n"; //< The previous line ends with the object count.

    for (int i = 0; i < (int) objectMetadataList.size(); ++i)
    {
        const auto& object = objectMetadataList.at(i);
        result.append(kIndent);

        result.append(toString(object));

        if (i < (int) objectMetadataList.size() - 1) //< Not the last object metadata.
            result.append("\n");
    }

    return result;
}

template<typename Time>
static QString toMsString(Time time)
{
    return QString::number(duration_cast<milliseconds>(time).count());
}

static microseconds vmsSystemTimeNow()
{
    return qnSyncTime //< Is null in unit tests.
        ? microseconds{qnSyncTime->currentUSecsSinceEpoch()}
        : duration_cast<microseconds>(system_clock::now().time_since_epoch());
}

/**
 * Before building the string, asserts that the input string does not contain `;` or non-printable
 * chars.
 */
static QString buildAdditionalInfoStr(const char* const func, const QString& additionalInfo)
{
    if (additionalInfo.isEmpty())
        return "";

    for (int i = 0; i < additionalInfo.size(); ++i)
    {
        int c = additionalInfo[i].unicode();
        NX_ASSERT(nx::kit::utils::isAsciiPrintable(c), nx::kit::utils::format(
            "%s(): additionalInfo contains non-printable char U+%04X at position %d: ",
            func, c, i, nx::kit::utils::toString(additionalInfo).c_str()));
        NX_ASSERT(c != ';', nx::kit::utils::format(
            "%s(): additionalInfo contains ';' at position %d: %s",
            func, i, nx::kit::utils::toString(additionalInfo).c_str()));
    }

    return "; additionalInfo: " + additionalInfo;
}

} // namespace

MetadataLogger::MetadataLogger(
    const QString& logFilePrefix,
    QnUuid deviceId,
    QnUuid engineId,
    nx::vms::api::StreamIndex streamIndex)
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
    if (m_outputFile.open(QIODevice::WriteOnly | QIODevice::Append))
        NX_INFO(this, "Logging metadata to file: %1", logFileName);
    else
        NX_WARNING(this, "Unable to open or create metadata log file: %1", logFileName);
}

MetadataLogger::~MetadataLogger()
{
    using namespace std::chrono;

    logLine(lm("Finished logging at %1 ms (VMS System time %2 ms)").args(
        toMsString(system_clock::now().time_since_epoch()),
        toMsString(vmsSystemTimeNow())));
}

void MetadataLogger::pushData(
    const QnConstAbstractMediaDataPtr& abstractMediaData,
    const QString& additionalInfo)
{
    if (!abstractMediaData || (!m_isAlwaysEnabled && !loggingIni().isLoggingEnabled()))
        return;

    if (abstractMediaData->dataType == QnAbstractMediaData::DataType::VIDEO)
    {
        const FrameInfo frameInfo{microseconds(abstractMediaData->timestamp)};
        logLine(buildFrameLogString(frameInfo, buildAdditionalInfoStr(__func__, additionalInfo)));
        m_prevFrameTimestamp = frameInfo.timestamp;
    }
    else if (abstractMediaData->dataType == QnAbstractMediaData::DataType::GENERIC_METADATA)
    {
        if (const ConstObjectMetadataPacketPtr objectMetadataPacket = fromCompressedMetadataPacket(
            std::dynamic_pointer_cast<const QnCompressedMetadata>(abstractMediaData)))
        {
            doPushObjectMetadata(__func__, *objectMetadataPacket, additionalInfo);
        }
    }
}

void MetadataLogger::pushFrameInfo(
    const FrameInfo& frameInfo,
    const QString& additionalInfo)
{
    if (!m_isAlwaysEnabled && !loggingIni().isLoggingEnabled())
        return;

    logLine(buildFrameLogString(frameInfo, buildAdditionalInfoStr(__func__, additionalInfo)));
    m_prevFrameTimestamp = frameInfo.timestamp;
}

void MetadataLogger::pushObjectMetadata(
    const ObjectMetadataPacket& metadataPacket,
    const QString& additionalInfo)
{
    doPushObjectMetadata(__func__, metadataPacket, additionalInfo);
}

void MetadataLogger::doPushObjectMetadata(
    const char* const func,
    const ObjectMetadataPacket& metadataPacket,
    const QString& additionalInfo)
{
    if (!m_isAlwaysEnabled && !loggingIni().isLoggingEnabled())
        return;

    m_isLoggingBestShot = false;
    if (metadataPacket.objectMetadataList.size() == 1)
    {
        if (metadataPacket.objectMetadataList[0].bestShot)
            m_isLoggingBestShot = true;
    }

    logLine(buildObjectMetadataLogString(
        metadataPacket, buildAdditionalInfoStr(func, additionalInfo)));

    if (!m_isLoggingBestShot)
        m_prevObjectMetadataPacketTimestamp = microseconds(metadataPacket.timestampUs);
}

QString MetadataLogger::buildFrameLogString(
    const FrameInfo& frameInfo,
    const QString& additionalInfoStr) const
{
    const microseconds vmsSystemTime = vmsSystemTimeNow();

    return "frameTimestampMs " + toMsString(frameInfo.timestamp) + ", "
        + "currentTimeMs " + toMsString(vmsSystemTime) + ", "
        + "diffFromPrevMs " + toMsString(frameInfo.timestamp - m_prevFrameTimestamp) + ", "
        + "diffFromCurrentTimeMs " + toMsString(frameInfo.timestamp - vmsSystemTime)
        + additionalInfoStr;
}

QString MetadataLogger::buildObjectMetadataLogString(
    const ObjectMetadataPacket& metadataPacket,
    const QString& additionalInfoStr) const
{
    const microseconds vmsSystemTime = vmsSystemTimeNow();
    const microseconds currentPacketTimestamp{metadataPacket.timestampUs};
    const microseconds diffFromPrev = currentPacketTimestamp - m_prevObjectMetadataPacketTimestamp;

    return "metadataTimestampMs " + toMsString(currentPacketTimestamp) + ", "
        + "currentTimeMs " + toMsString(vmsSystemTime) + ", "
        + "diffFromPrevMs " + toMsString(diffFromPrev) + ", "
        + "diffFromCurrentTimeMs " + toMsString(currentPacketTimestamp - vmsSystemTime)
        + additionalInfoStr
        + "; objects: " + QString::number(metadataPacket.objectMetadataList.size())
        + makeObjectsLogLinesIfNeeded(metadataPacket.objectMetadataList);
}

void MetadataLogger::logLine(QString lineStr)
{
    if (!lineStr.isEmpty() && lineStr.back() != '\n')
        lineStr.append('\n');

    if (m_outputFile.isOpen())
        m_outputFile.write(lineStr.toUtf8());
}

} // namespace nx::analytics
