// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metadata_logger.h"

#include <QtCore/QDir>

#include <nx/kit/utils.h>

#include <utils/common/synctime.h>
#include <nx/analytics/analytics_logging_ini.h>
#include <nx/utils/debug_helpers/debug_helpers.h>
#include <nx/utils/log/log.h>

namespace nx::analytics {

using namespace std::chrono;
using namespace nx::common::metadata;

namespace {

static const QString kIndent = "    ";

/** @return Empty string on error, having logged the error message. */
QString makeLogFileName(
    const QString& logFilePrefix,
    QnUuid deviceId,
    QnUuid engineId,
    nx::vms::api::StreamIndex streamIndex)
{
    const QString analyticsLoggingPath = loggingIni().analyticsLogPath;
    if (!NX_ASSERT(!analyticsLoggingPath.isEmpty()))
        return QString();

    const QString logDirectoryPath = nx::utils::debug_helpers::debugFilesDirectoryPath(
        analyticsLoggingPath);

    if (logDirectoryPath.isEmpty())
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

    const QDir dir(logDirectoryPath);
    return dir.absoluteFilePath(fileName);
}

static QString makeRectLogLinesIfNeeded(
    const std::vector<ObjectMetadata>& objectMetadataList,
    const QString& objectLogLinePrefix = QString())
{
    if (!loggingIni().logObjectMetadataDetails || objectMetadataList.empty())
        return "";

    QString result;
    for (int i = 0; i < (int) objectMetadataList.size(); ++i)
    {
        const auto& object = objectMetadataList.at(i);
        result.append(kIndent);

        if (!objectLogLinePrefix.isEmpty())
            result.append("(" + objectLogLinePrefix + ") ");

        result.append(toString(object));

        if (i < (int) objectMetadataList.size() - 1) //< Not the last object metadata.
            result.append("\n");
    }

    return result;
}

static QString makeBestShotDescriptionLines(const std::vector<ObjectMetadata>& objectMetadataList)
{
    std::vector<ObjectMetadata> bestShotMetadataList;
    std::vector<ObjectMetadata> nonBestShotObjectMetadataList;

    for (const auto& objectMetadata: objectMetadataList)
    {
        auto& packetList = objectMetadata.isBestShot()
            ? bestShotMetadataList
            : nonBestShotObjectMetadataList;

        packetList.push_back(objectMetadata);
    }

    NX_ASSERT(!bestShotMetadataList.empty());

    QString result = makeRectLogLinesIfNeeded(
        bestShotMetadataList,
        (bestShotMetadataList.size() == 1
            ? ""
            : "WARNING: Multiple best shot items in the object packet"));

    result += makeRectLogLinesIfNeeded(
        nonBestShotObjectMetadataList,
        "WARNING: Best shot packet contains non-best-shot metadata");

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

    logLine(nx::format("Finished logging at %1 ms (VMS System time %2 ms)").args(
        toMsString(system_clock::now().time_since_epoch()),
        toMsString(vmsSystemTimeNow())));
}

void MetadataLogger::pushData(
    const QnConstAbstractMediaDataPtr& abstractMediaData,
    const QString& additionalInfo)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

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
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!m_isAlwaysEnabled && !loggingIni().isLoggingEnabled())
        return;

    logLine(buildFrameLogString(frameInfo, buildAdditionalInfoStr(__func__, additionalInfo)));
    m_prevFrameTimestamp = frameInfo.timestamp;
}

void MetadataLogger::pushObjectMetadata(
    const ObjectMetadataPacket& metadataPacket,
    const QString& additionalInfo)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    doPushObjectMetadata(__func__, metadataPacket, additionalInfo);
}

void MetadataLogger::pushCustomMetadata(
    const nx::sdk::Ptr<nx::sdk::analytics::ICustomMetadataPacket>& customMetadata)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!m_isAlwaysEnabled && !loggingIni().isLoggingEnabled())
        return;

    if (!NX_ASSERT(customMetadata))
        return;

    logLine(buildCustomMetadataLogString(customMetadata));

    m_prevCustomMetadataTimestamp = microseconds(customMetadata->timestampUs());
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
        if (metadataPacket.objectMetadataList[0].isBestShot())
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

QString MetadataLogger::buildCustomMetadataLogString(
    const nx::sdk::Ptr<nx::sdk::analytics::ICustomMetadataPacket>& customMetadata)
{
    const microseconds vmsSystemTime = vmsSystemTimeNow();
    const microseconds customMetadataTimestamp(customMetadata->timestampUs());

    const std::string content(customMetadata->data(), customMetadata->dataSize());

    return "customMetadataTimestampMs " + toMsString(customMetadataTimestamp) + ", "
        + "currentTimeMs " + toMsString(vmsSystemTime) + ", "
        + "diffFromPrevMs " + toMsString(customMetadataTimestamp - m_prevCustomMetadataTimestamp)
        + ", "
        + "diffFromCurrentTimeMs " + toMsString(customMetadataTimestamp - vmsSystemTime) + ", "
        + "customMetadataCodec " + customMetadata->codec()
        + "; content:\n"
        + kIndent + QString::fromStdString(nx::kit::utils::toString(content));
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
        + (metadataPacket.containsBestShotMetadata()
            ? "; bestShot:\n" + makeBestShotDescriptionLines(metadataPacket.objectMetadataList)
            : "; objects: " + QString::number(metadataPacket.objectMetadataList.size()) + ":\n"
                + makeRectLogLinesIfNeeded(metadataPacket.objectMetadataList));
}

MetadataLogger::MetadataLogger(const QString& filename):
    m_isAlwaysEnabled(true)
{
    NX_CRITICAL(!filename.isEmpty());
    m_outputFile.setFileName(filename);
    const bool result = m_outputFile.open(QIODevice::WriteOnly);
    NX_CRITICAL(result);
}

void MetadataLogger::logLine(QString lineStr)
{
    if (!lineStr.isEmpty() && lineStr.back() != '\n')
        lineStr.append('\n');

    if (m_outputFile.isOpen())
    {
        m_outputFile.write(lineStr.toUtf8());
        m_outputFile.flush();
    }
}

} // namespace nx::analytics
