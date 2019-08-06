#pragma once

#include <vector>
#include <map>

#include <QtCore/QRectF>

namespace nx::analytics {

/**
 * Intended for experimenting and debugging.
 *
 * Loads an analytics (metadata) log into memory, and allows to get a rect by its timestamp.
 */
class MetadataLogParser
{
public:
    bool loadLogFile(QString logFilename);

    struct Packet
    {
        int64_t timestampMs;
        std::vector<QRectF> rects; /**< Each rect has coordinates in range [0..1]. */
    };

    /** @return Logged rects in range (lowerTimestampMs, upperTimestampMs]. */
    std::vector<const Packet*> packetsBetweenTimestamps(
        int64_t lowerTimestampMs, int64_t upperTimestampMs) const;

    /** Intended for debugging and testing. */
    bool saveToFile(const QString& filename) const;

private:
    bool parseFloat(
        std::istream& lineStream,
        int line,
        const std::string& expectedLabel,
        float* outValue) const;

    bool parseRectLine(
        std::istream& lineStream,
        int line,
        QRectF* outRect) const;

    bool parsePacketLine(
        std::istream& lineStream,
        int line,
        int64_t* outMetadataTimestampMs,
        int* outObjectCount) const;

    void sortPackets();

private:
    QString m_logFilename;
    std::list<Packet> m_packets; //< After loading, will be sorted by timestampMs.
};

} // namespace nx::analytics
