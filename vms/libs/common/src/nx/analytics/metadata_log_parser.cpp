#include "metadata_log_parser.h"

#include <QtCore/QString>

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>
#include <nx/utils/log/log.h>

namespace nx::analytics {

/**
 * Reads the text in form `label #,`, where `#` is a float number, skipping spaces and the
 * trailing comma (if any).
 */
bool MetadataLogParser::parseFloat(
    std::istream& lineStream,
    int line,
    const std::string& expectedLabel,
    float* outValue) const
{
    const auto warning =
        [this, line](const QString& message)
        {
            NX_WARNING(this, "Ignoring line %1 in log file %2: %3", line, m_logFilename, message);
            return false;
        };

    std::string labelFromLine;
    lineStream >> labelFromLine;
    if (lineStream.fail() || labelFromLine != expectedLabel)
        return warning(lm("Missing label \"%1\"").args(expectedLabel));

    lineStream >> *outValue;
    if (lineStream.fail())
        return warning(lm("Invalid float value for label \"%1\"").args(expectedLabel));

    // Skip trailing ",", if any.
    std::string trailing;
    lineStream >> trailing;
    if (!trailing.empty() && trailing != ",")
        return warning(lm("Unexpected trailing after label \"%1\"").args(expectedLabel));

    return true;
}

static const std::string kMetadataTimestampMsLabel = "metadataTimestampMs";
static const std::string kObjectsLabel = "objects:";

/**
 * Reads the text in form `metadataTimestampMs #, ...; objects: #:`, where `#` is the respective
 * number, and the trailing `:` can be missing.
 */
bool MetadataLogParser::parseRectLine(
    std::istream& lineStream,
    int line,
    QRectF* outRect) const
{
    const auto warning =
        [this, line](const QString& message)
        {
            NX_WARNING(this, "Ignoring rect line %1 in log file %2: %3",
                line, m_logFilename, message);
            return false;
        };

    float x, y, width, height;

    if (!parseFloat(lineStream, line, "x", &x))
        return false;
    if (x < 0 || x > 1)
        return warning(lm("\"x\" should be in range [0..1], but is %2").args(x));

    if (!parseFloat(lineStream, line, "y", &y))
    if (y < 0 || y > 1)
        return warning(lm("\"y\" should be in range [0..1], but is %2").args(y));

    if (!parseFloat(lineStream, line, "width", &width))
        return false;
    if (width < 0 || x + width > 1)
        return warning(lm("\"width\" should be in range [0..1 - x], but is %2").args(width));

    if (!parseFloat(lineStream, line, "height", &height))
        return false;
    if (height < 0 || y + height > 1)
        return warning(lm("\"height\" should be in range [0..1 - y], but is %2").args(width));

    *outRect = QRectF(x, y, width, height);
    return true;
}

/**
 * Parsees the text in form `metadataTimestampMs #, ...; objects: #:`, where `#` is the respective
 * number, and the trailing `:` can be missing.
 */
bool MetadataLogParser::parsePacketLine(
    std::istream& lineStream,
    int line,
    int64_t* outMetadataTimestampMs,
    int* outObjectCount) const
{
    const auto warning =
        [this, line](const QString& message)
        {
            NX_WARNING(this, "Ignoring packet line %1 in log file %2: %3",
                line, m_logFilename, message);
            return false;
        };

    std::string metadataTimestampMsLabel;
    lineStream >> metadataTimestampMsLabel;
    if (metadataTimestampMsLabel != kMetadataTimestampMsLabel)
        return false; //< Not an error - the line is not a packet line.
    if (lineStream.fail())
        return warning(lm("Missing label \"%1\"").args(kMetadataTimestampMsLabel));

    lineStream >> *outMetadataTimestampMs;
    if (lineStream.fail())
        return warning(lm("Invalid int64 value for label \"%1\"").args(kMetadataTimestampMsLabel));

    // Skip everything until the kObjectsLabel which should come as the last item.
    for (;;)
    {
        std::string label;
        lineStream >> label;
        if (label == kObjectsLabel)
            break;
        if (label.empty() || !lineStream)
            return warning(lm("Missing label \"%1\"").args(kObjectsLabel));
    }

    lineStream >> *outObjectCount;
    if (lineStream.fail())
        return warning(lm("Invalid object count value after label \"%1\"").args(kObjectsLabel));

    // Skip trailing ":", if any.
    std::string trailing;
    lineStream >> trailing;
    if (!trailing.empty() && trailing != ":")
        return warning(lm("Unexpected trailing after label \"%1\"").args(kObjectsLabel));

    return true;
}

void MetadataLogParser::sortPackets()
{
    m_packets.sort(
        [](const Packet& first, const Packet& second)
        {
            return first.timestampMs < second.timestampMs;
        });
}

bool MetadataLogParser::loadLogFile(QString logFilename)
{
    const auto error =
        [this](const QString& message)
        {
            NX_ERROR(this, "Unable to read log file %1: %2", m_logFilename, message);
            return false;
        };

    m_logFilename = std::move(logFilename);

    std::ifstream f(m_logFilename.toStdString().c_str());
    if (!f.is_open())
        return error("Cannot open the file");

    int line = 0;
    std::string packetLineString;
    while (std::getline(f, packetLineString))
    {
        ++line;
        if (packetLineString.empty())
            continue;

        std::istringstream packetLineStream(packetLineString);
        int objectCount = -1;
        int64_t metadataTimestampMs = -1;
        if (!parsePacketLine(packetLineStream, line, &metadataTimestampMs, &objectCount))
            continue; //< Ignore non-packet lines, as well as bad packet lines.

        std::vector<QRectF> rects;
        for (int i = 0; i < objectCount; ++i)
        {
            std::string rectLineString;
            ++line;
            if (!std::getline(f, rectLineString))
                return error(lm("Unable to read line %1 for rect #%2").args(line, objectCount));
            std::istringstream rectLineStream(rectLineString);
            QRectF rect;
            if (parseRectLine(rectLineStream, line, &rect)) //< Ignore unparsed rect lines.
                rects.push_back(rect);
        }

        m_packets.push_back({metadataTimestampMs, std::move(rects)});
    }

    if (!f.eof() || f.bad())
        return error("I/O error");

    sortPackets();
    return true;
}

std::vector<const MetadataLogParser::Packet*> MetadataLogParser::packetsBetweenTimestamps(
    int64_t lowerTimestampMs, int64_t upperTimestampMs) const
{
    if (!NX_ASSERT(lowerTimestampMs <= upperTimestampMs))
        return {};

    std::vector<const Packet*> result;

    const auto reverseCompare =
        [](const Packet& first, const Packet& second)
        {
            return first.timestampMs > second.timestampMs;
        };

    // The rightmost packet with timestamp less or equal to the upper one.
    const auto& upper = std::lower_bound(
        m_packets.crbegin(), m_packets.crend(), Packet{upperTimestampMs, {}}, reverseCompare);

    // The rightmost packet with the timestamp less or equal to the lower one.
    auto lower = std::lower_bound(
        m_packets.crbegin(), m_packets.crend(), Packet{lowerTimestampMs, {}}, reverseCompare);

    while (lower != upper)
    {
        --lower; //< Move left-to-right to the next packet.
        result.push_back(&(*lower));
    }

    return result;
}

bool MetadataLogParser::saveToFile(const QString& filename) const
{
    QFile f(filename);
    bool success = true;

    const auto logLine =
        [this, &f, &success, &filename](QString lineStr)
        {
            if (!lineStr.isEmpty() && lineStr.back() != '\n')
                lineStr.append('\n');

            if (f.write(lineStr.toUtf8()) <= 0)
            {
                success = false;
                NX_WARNING(this, "Unable to write to file %1", filename);
            }
        };

    if (!f.open(QIODevice::WriteOnly))
    {
        NX_ERROR(this, "Unable to open output file %1", filename);
        return false;
    }

    for (const auto& packet: m_packets)
    {
        logLine(
            QString::fromStdString(kMetadataTimestampMsLabel) + " "
            + QString::number(packet.timestampMs)
            + "; "
            + QString::fromStdString(kObjectsLabel)
            + " "
            + QString::number(packet.rects.size())
            + (packet.rects.empty() ? "" : ":")
        );

        for (const auto& rect: packet.rects)
        {
            logLine(lm("    x %1, y %2, width %3, height %4").args(
                rect.x(), rect.y(), rect.width(), rect.height()));
        }
    }

    return success;
}

} // namespace nx::analytics
